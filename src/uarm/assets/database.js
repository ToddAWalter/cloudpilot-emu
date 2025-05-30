// @ts-check

/**
 * @typedef {{name: string, content: Uint8Array}} Image
 * @typedef {{name: string, id: string, mounted: boolean, content: Uint8Array}} CardImage
 * @typedef {{scheduledPageCount: number, scheduledPages: ArrayBuffer, pagePool: ArrayBuffer, crc?: number}} SnapshotPages
 * @typedef {{nand?: SnapshotPages, sd?: SnapshotPages, ram?: SnapshotPages, cardId? :string, savestate: ArrayBuffer, ramSize: number}} Snapshot
 */

const DB_NAME = 'cp-uarm';
const DB_VERSION = 6;

const SIZE_NAND = 528 * 2 * 1024 * 32;
const PAGE_SIZE_NAND = 528 * 8;
const EMPTY_VALUE_NAND = 0xffffffff;

const PAGE_SIZE_SD = 8 * 1024;
const EMPTY_VALUE_SD = 0;

const PAGE_SIZE_RAM = 512;
const EMPTY_VALUE_RAM = 0;

const OBJECT_STORE_KVS = 'kvs';
const OBJECT_STORE_NAND = 'nand';
const OBJECT_STORE_SD = 'sd';
const OBJECT_STORE_RAM = 'ram';
const OBJECT_STORE_LOCK = 'lock';

const KVS_ROM_NOR = 'romNor';
const KVS_ROM_NAND = 'romNand';
const KVS_SD_IMAGE = 'sdImage';
const KVS_SD_NAME = 'sdName';
const KVS_SD_SIZE = 'sdSize';
const KVS_SD_CRC = 'sdCrc';
const KVS_NAND_NAME = 'nandName';
const KVS_NAND_CRC = 'nandCrc';
const KVS_RAM_CRC = 'ramCrc';
const KVS_CARD_ID = 'cardId';
const KVS_CARD_MOUNTED = 'cardMounted';
const KVS_SAVESTATE = 'savestate';
const KVS_RAM_SIZE = 'ramSize';
export const KVS_MIPS = 'mips';
export const KVS_MAX_LOAD = 'maxLoad';

/**
 * @returns {string}
 */
function randomId() {
    const bytes = new Uint8Array(16);
    crypto.getRandomValues(bytes);

    return Array.from(bytes)
        .map((x) => x.toString(16))
        .map((x) => x.padStart(2, '0'))
        .join('');
}

/**
 *
 * @param {Uint8Array} compressed
 * @returns {Uint8Array}
 */
function decompressCard(compressed) {
    let pos = 0;
    const len = compressed[pos++] | (compressed[pos++] << 8) | (compressed[pos++] << 16) | (compressed[pos++] << 24);

    const image = new Uint8Array(len);

    for (let i = 0; pos < compressed.length; i++) {
        const blockType = compressed[pos++];

        let blockLen = 0x2000;
        if (blockType & 0x80) {
            blockLen = compressed[pos++] | (compressed[pos++] << 8);
        }

        const block = image.subarray(i << 13, (i + 1) << 13);

        if (blockType & 0x01) block.fill(compressed[pos++]);
        else {
            block.set(compressed.subarray(pos, pos + block.length));
            pos += block.length;
        }
    }

    if (pos !== compressed.length) throw new Error('compressed card image corrupt');

    return image;
}

/**
 *
 * @param {Uint32Array} page
 * @returns {Uint32Array | number}
 */
export function compressPage(page) {
    const fst = page[0];
    const len = page.length;

    for (let i = 1; i < len; i++) {
        if (page[i] !== fst) return page;
    }

    return fst;
}

/**
 * @template T
 * @param {IDBRequest<T> | IDBTransaction} txOrRequest
 * @returns {Promise<T>}
 */
function complete(txOrRequest) {
    return new Promise((resolve, reject) => {
        const request = /** @type IDBRequest */ (txOrRequest);
        const tx = /** @type IDBTransaction */ (txOrRequest);

        if (!!tx.abort) {
            tx.oncomplete = () => resolve(/** @type T */ (undefined));
        } else {
            request.onsuccess = () => resolve(request.result);
        }

        txOrRequest.onerror = () => reject(txOrRequest.error);
    });
}

/**
 *
 * @param {number} size
 * @param {number} pageSize
 * @param {string} objectStore
 * @param {number} emptyValue
 * @param {IDBTransaction} tx
 * @returns {Promise<Uint8Array>}
 */
async function getPagedData(size, pageSize, objectStore, emptyValue, tx) {
    if (size % pageSize) throw new Error('invalid size');

    const pagesTotal = (size / pageSize) | 0;
    const pageSize4 = pageSize >>> 2;
    const data32 = new Uint32Array(size >>> 2);
    const data8 = new Uint8Array(data32.buffer, 0, size);
    data32.fill(emptyValue);

    const store = tx.objectStore(objectStore);

    const cursorRequest = store.openCursor();
    return new Promise((resolve, reject) => {
        cursorRequest.onsuccess = () => {
            const cursor = cursorRequest.result;
            if (!cursor) return resolve(data8);

            const pageIndex = /** @type number */ (cursor.key);
            if (pageIndex > pagesTotal) return reject(new Error(`page index ${pageIndex} out of bounds`));

            try {
                const page32 = data32.subarray(pageIndex * pageSize4, (pageIndex + 1) * pageSize4);
                if (typeof cursor.value === 'number') page32.fill(cursor.value);
                else page32.set(cursor.value);
            } catch (e) {
                reject(e);
            }

            cursor.continue();
        };
    });
}

/**
 * @param {Uint8Array | undefined} data8
 * @param {number} pageSize
 * @param {string} objectStore
 * @param {number} emptyValue
 * @param {IDBTransaction} tx
 * @returns {void}
 */
function putPagedData(data8, pageSize, objectStore, emptyValue, tx) {
    if (data8 && data8.length % pageSize) throw new Error('invalid paged data size');

    const store = tx.objectStore(objectStore);
    store.clear();

    if (!data8) return;

    const pagesTotal = (data8.length / pageSize) | 0;
    const pageSize4 = pageSize >>> 2;
    const data32 = new Uint32Array(data8.buffer, 0, data8.length >>> 2);

    for (let iPage = 0; iPage < pagesTotal; iPage++) {
        const compressedPage = compressPage(data32.subarray(iPage * pageSize4, (iPage + 1) * pageSize4));
        if (compressedPage === emptyValue) continue;

        store.put(typeof compressedPage === 'number' ? compressedPage : compressedPage.slice(), iPage);
    }
}

/**
 *
 * @param {IDBOpenDBRequest} request
 * @returns {Promise<void>}
 */
async function migrateNand(request) {
    const tx = request.transaction;
    if (!tx) throw new Error('no version change transaction');

    const storeKvs = tx.objectStore(OBJECT_STORE_KVS);

    const nandImage = /** @type Image | undefined*/ (await complete(storeKvs.get(KVS_ROM_NAND)));
    if (!nandImage) return;

    storeKvs.delete(KVS_ROM_NAND);

    if (nandImage.content.length % PAGE_SIZE_NAND === 0) {
        storeKvs.put(nandImage.name, KVS_NAND_NAME);
        putPagedData(nandImage.content, PAGE_SIZE_NAND, OBJECT_STORE_NAND, EMPTY_VALUE_NAND, tx);

        console.log('migrating NAND image');
    } else {
        console.warn('removing NAND image with invalid length');
    }
}

/**
 *
 * @param {IDBOpenDBRequest} request
 * @returns {Promise<void>}
 */
async function migrateSd(request) {
    const tx = request.transaction;
    if (!tx) throw new Error('no version change transaction');

    const storeKvs = tx.objectStore(OBJECT_STORE_KVS);

    const sdImage = /** @type Image | undefined*/ (await complete(storeKvs.get(KVS_SD_IMAGE)));
    if (!sdImage) return;

    sdImage.content = decompressCard(sdImage.content);
    storeKvs.delete(KVS_SD_IMAGE);

    if (sdImage.content.length % PAGE_SIZE_SD === 0) {
        storeKvs.put(sdImage.name, KVS_SD_NAME);
        storeKvs.put(sdImage.content.length, KVS_SD_SIZE);
        putPagedData(sdImage.content, PAGE_SIZE_SD, OBJECT_STORE_SD, EMPTY_VALUE_SD, tx);

        console.log('migrating SD image');
    } else {
        console.warn('removing SD image with invalid length');
    }
}

/**
 *
 * @param {IDBOpenDBRequest} request
 * @returns {Promise<void>}
 */
async function migrateCardIdAndMountFlag(request) {
    const tx = request.transaction;

    if (!tx) throw new Error('no version change transaction');

    const storeKvs = tx.objectStore(OBJECT_STORE_KVS);
    const name = /** @type string | undefined*/ (await complete(storeKvs.get(KVS_SD_NAME)));

    if (name !== undefined) {
        storeKvs.put(true, KVS_CARD_MOUNTED);
        storeKvs.put(randomId(), KVS_CARD_ID);
    } else {
        storeKvs.put(false, KVS_CARD_MOUNTED);
    }
}

/**
 *
 * @returns {Promise<IDBDatabase>}
 */
function initdDb() {
    return new Promise((resolve, reject) => {
        const request = indexedDB.open(DB_NAME, DB_VERSION);

        request.onerror = () => reject('failed to open database');
        request.onblocked = () => reject('necessary db update blocked by open app');
        request.onupgradeneeded = (e) => {
            const db = request.result;

            if (e.oldVersion < 1) {
                db.createObjectStore(OBJECT_STORE_KVS);
            }

            if (e.oldVersion < 2) {
                db.createObjectStore(OBJECT_STORE_LOCK);
            }

            if (e.oldVersion < 3) {
                db.createObjectStore(OBJECT_STORE_NAND);
                migrateNand(request);
            }

            if (e.oldVersion < 4) {
                db.createObjectStore(OBJECT_STORE_SD);
                migrateSd(request);
            }

            if (e.oldVersion < 5) {
                db.createObjectStore(OBJECT_STORE_RAM);
                migrateSd(request);
            }

            if (e.oldVersion < 6) {
                migrateCardIdAndMountFlag(request);
            }
        };

        request.onsuccess = () => resolve(request.result);
    });
}

/**
 *
 * @param {IDBDatabase} db
 * @returns {Promise<string>}
 */
async function acquireLock(db) {
    const lockToken = randomId();
    const tx = db.transaction([OBJECT_STORE_LOCK], 'readwrite');

    await complete(tx.objectStore(OBJECT_STORE_LOCK).put(lockToken, 0));

    return lockToken;
}

export class Database {
    /**
     *
     * @param {IDBDatabase} db
     * @param {string} lockToken
     */
    constructor(db, lockToken) {
        this.db = db;
        this.lockToken = lockToken;
        this.lockLost = false;
        this.pagePoolNand = /** @type Array<Uint32Array> */ (new Array());
        this.pagePoolSd = /** @type Array<Uint32Array> */ (new Array());
        this.pagePoolRam = /** @type Array<Uint32Array> */ (new Array());
    }

    /**
     *
     * @returns {Promise<Database>}
     */
    static async create() {
        const db = await initdDb();
        const lockToken = await acquireLock(db);

        return new Database(db, lockToken);
    }

    /**
     *
     * @param {IDBTransaction} tx
     * @returns {Promise<void>}
     */
    async assertLock(tx) {
        const lockToken = /** @type string */ (await complete(tx.objectStore(OBJECT_STORE_LOCK).get(0)));

        if (lockToken !== this.lockToken) {
            tx.abort();

            if (!this.lockLost) {
                setTimeout(
                    () => alert('CloudpilotEmu has been opened in another tab. No more data will be persisted.'),
                    0
                );

                this.db.close();
            }

            this.lockLost = true;
            throw new Error('page lock lost');
        }
    }

    /**
     *
     * @param  {...string} stores
     * @returns {Promise<IDBTransaction>}
     */
    async tx(...stores) {
        const tx = this.db.transaction([...stores, OBJECT_STORE_LOCK], 'readwrite', { durability: 'relaxed' });
        await this.assertLock(tx);

        return tx;
    }

    /**
     *
     * @param {string} key
     * @returns {Promise<any>}
     */
    async kvsGet(key) {
        const tx = await this.tx(OBJECT_STORE_KVS);

        return complete(tx.objectStore(OBJECT_STORE_KVS).get(key));
    }

    /**
     *
     * @param {string} key
     * @param {any} value
     */
    async kvsPut(key, value) {
        const tx = await this.tx(OBJECT_STORE_KVS);

        tx.objectStore(OBJECT_STORE_KVS).put(value, key);

        await complete(tx);
    }

    /**
     *
     * @param {string} key
     * @returns {Promise<void>}
     */
    async kvsDelete(key) {
        const tx = await this.tx(OBJECT_STORE_KVS);

        await complete(tx.objectStore(OBJECT_STORE_KVS).delete(key));
    }

    /**
     *
     * @param {IDBTransaction} tx
     */
    resetSession(tx) {
        const kvsStore = tx.objectStore(OBJECT_STORE_KVS);

        tx.objectStore(OBJECT_STORE_RAM).clear();
        kvsStore.delete(KVS_SAVESTATE);
        kvsStore.delete(KVS_RAM_SIZE);
        kvsStore.delete(KVS_RAM_CRC);
    }

    /**
     *
     * @returns {Promise<Image>}
     */
    getNor() {
        return this.kvsGet(KVS_ROM_NOR);
    }

    /**
     *
     * @param {Image} nor
     * @returns {Promise<void>}
     */
    async putNor(nor) {
        const tx = await this.tx(OBJECT_STORE_KVS, OBJECT_STORE_RAM);

        this.putNorTx(tx, nor);
        this.resetSession(tx);

        await complete(tx);
    }

    /**
     *
     * @param {IDBTransaction} tx
     * @param {Image} nor
     */
    putNorTx(tx, nor) {
        tx.objectStore(OBJECT_STORE_KVS).put(nor, KVS_ROM_NOR);
    }

    /**
     * @param {boolean} crcCheck
     * @param {number} size
     * @returns {Promise<Uint8Array | undefined>}
     */
    async getRam(crcCheck, size) {
        if (size === 0) return undefined;

        const tx = await this.tx(OBJECT_STORE_RAM, OBJECT_STORE_KVS);
        const storeKvs = tx.objectStore(OBJECT_STORE_KVS);

        const content = await getPagedData(size + 32 * 1024, PAGE_SIZE_RAM, OBJECT_STORE_RAM, EMPTY_VALUE_RAM, tx);

        const crc = await complete(storeKvs.get(KVS_RAM_CRC));
        const crc32 = /** @type any */ (window).crc32;

        if (crcCheck && typeof crc === 'number') {
            if (crc !== crc32(content)) {
                alert(`CRC mismatch on RAM load!`);
            } else {
                console.log('RAM CRC OK');
            }
        }

        return content;
    }

    /**
     *
     * @param {IDBTransaction} tx
     * @param {Uint8Array | undefined} ram
     */
    putRamTx(tx, ram) {
        const storeKvs = tx.objectStore(OBJECT_STORE_KVS);

        storeKvs.delete(KVS_RAM_CRC);
        putPagedData(ram, PAGE_SIZE_RAM, OBJECT_STORE_RAM, EMPTY_VALUE_RAM, tx);
    }

    /**
     * @param {boolean} crcCheck
     * @returns {Promise<Image | undefined>}
     */
    async getNand(crcCheck) {
        const tx = await this.tx(OBJECT_STORE_NAND, OBJECT_STORE_KVS);
        const storeKvs = tx.objectStore(OBJECT_STORE_KVS);

        const name = /** @type string */ (await complete(storeKvs.get(KVS_NAND_NAME)));
        if (!name) return;

        const content = await getPagedData(SIZE_NAND, PAGE_SIZE_NAND, OBJECT_STORE_NAND, EMPTY_VALUE_NAND, tx);

        const crc = await complete(storeKvs.get(KVS_NAND_CRC));
        const crc32 = /** @type any */ (window).crc32;

        if (crcCheck && typeof crc === 'number') {
            if (crc !== crc32(content)) {
                alert(`CRC mismatch on NAND load!`);
            } else {
                console.log('NAND CRC OK');
            }
        }

        return { name, content };
    }

    /**
     *
     * @param {Image} nand
     */
    async putNand(nand) {
        const tx = await this.tx(OBJECT_STORE_NAND, OBJECT_STORE_KVS, OBJECT_STORE_RAM);

        this.putNandTx(tx, nand);
        this.resetSession(tx);

        await complete(tx);
    }

    /**
     *
     * @param {IDBTransaction} tx
     * @param {Image} nand
     */
    putNandTx(tx, nand) {
        if (nand.content.length !== SIZE_NAND) {
            throw new Error('bad NAND size');
        }

        const storeKvs = tx.objectStore(OBJECT_STORE_KVS);

        storeKvs.put(nand.name, KVS_NAND_NAME);
        storeKvs.delete(KVS_NAND_CRC);

        putPagedData(nand.content, PAGE_SIZE_NAND, OBJECT_STORE_NAND, EMPTY_VALUE_NAND, tx);
    }

    async clearNand() {
        const tx = await this.tx(OBJECT_STORE_NAND, OBJECT_STORE_KVS, OBJECT_STORE_RAM);
        const storeKvs = tx.objectStore(OBJECT_STORE_KVS);

        storeKvs.put('[blank]', KVS_NAND_NAME);
        storeKvs.delete(KVS_NAND_CRC);

        tx.objectStore(OBJECT_STORE_NAND).clear();
        this.resetSession(tx);

        await complete(tx);
    }

    /**
     * @param {boolean} crcCheck
     * @returns {Promise <CardImage | undefined>}
     */
    async getSd(crcCheck) {
        const tx = await this.tx(OBJECT_STORE_SD, OBJECT_STORE_KVS);
        const storeKvs = tx.objectStore(OBJECT_STORE_KVS);

        const id = /** @type string */ (await complete(storeKvs.get(KVS_CARD_ID)));
        if (!id) return;

        const name = /** @type string */ (await complete(storeKvs.get(KVS_SD_NAME)));
        const mounted = /** @type boolean */ (await complete(storeKvs.get(KVS_CARD_MOUNTED)));

        const size = /** @type number */ (await complete(storeKvs.get(KVS_SD_SIZE)));
        if (size === undefined) return;

        const content = await getPagedData(size, PAGE_SIZE_SD, OBJECT_STORE_SD, EMPTY_VALUE_SD, tx);

        const crc = await complete(storeKvs.get(KVS_SD_CRC));
        const crc32 = /** @type any */ (window).crc32;

        if (crcCheck && typeof crc === 'number') {
            if (crc !== crc32(content)) {
                alert(`CRC mismatch on SD load!`);
            } else {
                console.log('SD CRC OK');
            }
        }

        return { name, id, mounted, content };
    }

    /**
     *
     * @param {Image} sd
     * @param {boolean} mounted
     * @returns {Promise<CardImage>}
     */
    async putSd(sd, mounted) {
        const tx = await this.tx(OBJECT_STORE_SD, OBJECT_STORE_KVS);
        const storeKvs = tx.objectStore(OBJECT_STORE_KVS);
        const id = randomId();

        storeKvs.put(id, KVS_CARD_ID);
        storeKvs.put(mounted, KVS_CARD_MOUNTED);
        storeKvs.put(sd.name, KVS_SD_NAME);
        storeKvs.put(sd.content.length, KVS_SD_SIZE);
        storeKvs.delete(KVS_SD_CRC);
        putPagedData(sd.content, PAGE_SIZE_SD, OBJECT_STORE_SD, EMPTY_VALUE_SD, tx);

        await complete(tx);

        return { ...sd, id, mounted };
    }

    /**
     *
     * @param {boolean} mounted
     * @returns {Promise<void>}
     */
    async setCardMounted(mounted) {
        const tx = await this.tx(OBJECT_STORE_KVS);
        const storeKvs = tx.objectStore(OBJECT_STORE_KVS);

        storeKvs.put(mounted, KVS_CARD_MOUNTED);

        await complete(tx);
    }

    /**
     * @returns {Promise<Uint8Array | undefined>}
     */
    getSavestate() {
        return this.kvsGet(KVS_SAVESTATE);
    }

    /**
     *
     * @returns {Promise<void>}
     */
    async removeSavestate() {
        await this.kvsDelete(KVS_SAVESTATE);
    }

    /**
     * @returns {Promise<number>}
     */
    async getRamSize() {
        const ramSize = await this.kvsGet(KVS_RAM_SIZE);
        if (ramSize !== undefined) return ramSize;

        if ((await this.kvsGet(KVS_SAVESTATE)) !== undefined) return 16 * 1024 * 1024;

        return 0;
    }

    /**
     *
     * @param {number} ramSize
     * @param {Image} nor
     * @param {Image} nand
     * @param {Uint8Array | undefined} ram
     * @param {Uint8Array | undefined} savestate
     *
     * @returns {Promise<void>}
     */
    async importSession(ramSize, nor, nand, ram, savestate) {
        const tx = await this.tx(OBJECT_STORE_KVS, OBJECT_STORE_NAND, OBJECT_STORE_RAM);
        const storeKvs = tx.objectStore(OBJECT_STORE_KVS);

        this.resetSession(tx);

        this.putNorTx(tx, nor);
        this.putNandTx(tx, nand);
        this.putRamTx(tx, ram);
        storeKvs.put(savestate, KVS_SAVESTATE);
        storeKvs.put(ramSize, KVS_RAM_SIZE);

        await complete(tx);
    }

    /**
     *
     * @param {Snapshot} snapshot
     * @returns {Promise<void>}
     */
    async storeSnapshot(snapshot) {
        const tx = await this.tx(OBJECT_STORE_NAND, OBJECT_STORE_SD, OBJECT_STORE_RAM, OBJECT_STORE_KVS);
        const kvs = tx.objectStore(OBJECT_STORE_KVS);

        const cardId = /** @type string */ (await complete(kvs.get(KVS_CARD_ID)));
        if (snapshot.sd && cardId !== snapshot.cardId) {
            throw new Error(`snapshot aborted: card ID mismatch ${cardId} vs. ${snapshot.cardId}`);
        }

        kvs.put(snapshot.ramSize, KVS_RAM_SIZE);
        kvs.put(new Uint8Array(snapshot.savestate), KVS_SAVESTATE);

        if (snapshot.nand) {
            this.storeSnapshotPages(snapshot.nand, tx, PAGE_SIZE_NAND, this.pagePoolNand, OBJECT_STORE_NAND);
            kvs.put(snapshot.nand.crc, KVS_NAND_CRC);
        }

        if (snapshot.sd) {
            this.storeSnapshotPages(snapshot.sd, tx, PAGE_SIZE_SD, this.pagePoolSd, OBJECT_STORE_SD);
            kvs.put(snapshot.sd.crc, KVS_SD_CRC);
        }

        if (snapshot.ram) {
            this.storeSnapshotPages(snapshot.ram, tx, PAGE_SIZE_RAM, this.pagePoolRam, OBJECT_STORE_RAM);
            kvs.put(snapshot.ram.crc, KVS_RAM_CRC);
        }

        await complete(tx);
    }

    /**
     *
     * @param {SnapshotPages} snapshot
     * @param {IDBTransaction} tx
     * @param {number} pageSize
     * @param {string} objectStore
     * @param {Array<Uint32Array>} retainedPagePool
     * @returns {void}
     */
    storeSnapshotPages(snapshot, tx, pageSize, retainedPagePool, objectStore) {
        const store = tx.objectStore(objectStore);
        const scheduledPageCount = snapshot.scheduledPageCount;
        const scheduledPages = new Uint32Array(snapshot.scheduledPages);
        const pagePool = new Uint32Array(snapshot.pagePool);
        const pageSize4 = pageSize >>> 2;

        let iPool = 0;
        for (let iPage = 0; iPage < scheduledPageCount; iPage++) {
            let compressedPage = compressPage(pagePool.subarray(iPage * pageSize4, (iPage + 1) * pageSize4));

            if (typeof compressedPage !== 'number') {
                if (retainedPagePool.length >= iPool) retainedPagePool.push(new Uint32Array(pageSize4));
                const pageBuffer = retainedPagePool[iPool++];

                pageBuffer.set(compressedPage);
                compressedPage = pageBuffer;
            }

            store.put(compressedPage, scheduledPages[iPage]);
        }
    }
}
