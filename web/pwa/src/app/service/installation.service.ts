import { Injectable } from '@angular/core';
import { DbInstallResult } from '@common/bridge/Cloudpilot';
import { FsTools } from '@common/bridge/FSTools';
import { ZipfileWalkerState } from '@common/bridge/ZipfileWalker';
import { LoadingController } from '@ionic/angular';

import { concatFilenames } from '@pwa/helper/filename';

import { AlertService } from './alert.service';
import { CloudpilotService } from './cloudpilot.service';
import { EmulationService } from './emulation.service';
import { FileDescriptor } from './file.service';
import { FsToolsService } from './fstools.service';
import { SnapshotService } from './snapshot.service';

const ZIP_SIZE_LIMIT = 32 * 1024 * 1024;
const SNAPSHOT_LIMIT = 4 * 1024 * 1024;

function isInstallable(filename: string) {
    return /\.(prc|pdb|pqa)$/i.test(filename);
}

function describeError(code: DbInstallResult): string {
    switch (code) {
        case DbInstallResult.failedCouldNotOverwrite:
            return 'database exists on device and could not be overwritten';

        case DbInstallResult.failureDbIsCorrupt:
            return 'database file is corrupt';

        case DbInstallResult.failureDbIsOpen:
            return 'database is open on device';

        case DbInstallResult.failureInternal:
            return 'internal PalmOS error';

        case DbInstallResult.failureNotEnoughMemory:
            return 'not enough memory on device';

        default:
            return 'unknown reason';
    }
}

function getDatabaseName(data: Uint8Array): string {
    if (data.length < 32) throw new Error('not a database');

    let iTerminator: number;
    for (iTerminator = 0; iTerminator < 32; iTerminator++) {
        if (data[iTerminator] === 0) break;
    }

    if (iTerminator > 31) throw new Error('not a database');

    return new TextDecoder('latin1', { fatal: false }).decode(data.subarray(0, iTerminator));
}

class InstallationContext {
    constructor(
        private cloudpilotService: CloudpilotService,
        private fstools: FsTools,
        private alertService: AlertService,
        private snapshotService: SnapshotService,
        private files: Array<FileDescriptor>,
    ) {}

    async run(): Promise<[Array<string>, Array<string>, Array<string>]> {
        for (const file of this.files) {
            let content: Uint8Array;
            try {
                content = await file.getContent();
            } catch (e) {
                console.warn(e);

                this.filesFail.push(file.name);
                continue;
            }

            if (/\.zip/i.test(file.name) && content.length < ZIP_SIZE_LIMIT) {
                try {
                    await this.installZip(file);
                    // eslint-disable-next-line @typescript-eslint/no-unused-vars
                } catch (e) {
                    this.filesFail.push(file.name);
                }
            } else if (isInstallable(file.name)) {
                await this.installOne(file.name, content);
            } else {
                this.filesFail.push(file.name);
            }
        }

        return [this.filesSuccess, this.filesRequireReset, this.filesFail];
    }

    private async installOne(name: string, content: Uint8Array): Promise<void> {
        const cloudpilot = await this.cloudpilotService.cloudpilot;
        const code = await cloudpilot.installDb(content);

        switch (code) {
            case DbInstallResult.needsReboot:
                this.filesRequireReset.push(name);
                this.filesSuccess.push(name);
                this.sizeInstalledSinceLastsnapshot += content.length;
                break;

            case DbInstallResult.success:
                this.filesSuccess.push(name);
                this.sizeInstalledSinceLastsnapshot += content.length;
                break;

            default:
                this.filesFail.push(name);
                await this.reportError(name, code);

                break;
        }

        if (code === DbInstallResult.needsReboot || code === DbInstallResult.success) {
            try {
                const dbName = getDatabaseName(content);

                if (this.installedDatabases.has(dbName)) {
                    await this.errorDialog(
                        'Duplicate item',
                        `${name} overwrites previously installed database ${this.installedDatabases.get(dbName)}`,
                    );
                }

                this.installedDatabases.set(dbName, name);
                // eslint-disable-next-line @typescript-eslint/no-unused-vars
            } catch (e) {
                // ignore stuff that is not a database
            }
        }

        if (this.sizeInstalledSinceLastsnapshot > SNAPSHOT_LIMIT) {
            await this.snapshotService.triggerSnapshot();
            this.sizeInstalledSinceLastsnapshot = 0;
        }

        await new Promise((r) => setTimeout(r, 0));
    }

    private async installZip(file: FileDescriptor): Promise<void> {
        const walker = await this.fstools.createZipfileWalker(await file.getContent());

        let installed = 0;

        while (walker.GetState() === ZipfileWalkerState.open) {
            const name = walker.GetCurrentEntryName().replace(/^.*\//, '');

            if (!isInstallable(walker.GetCurrentEntryName())) {
                walker.Next();
                continue;
            }
            const content = walker.GetCurrentEntryContent();
            if (content) {
                await this.installOne(name, content);
                installed++;
            } else {
                this.filesFail.push(name);
            }

            walker.Next();
        }

        if (installed === 0) throw new Error('no installable files in archive');
    }

    private reportError(file: string, code: DbInstallResult): Promise<void> {
        return this.errorDialog('Item failed to install', `Could not install ${file}: ${describeError(code)}.`);
    }

    private errorDialog(header: string, message: string): Promise<void> {
        if (this.skipErrors) return Promise.resolve();

        return this.alertService
            .messageWithChoice(header, message, 'Skip any remaining errors', false, {}, 'Continue')
            .then((choice) => void (this.skipErrors = choice));
    }

    private filesSuccess: Array<string> = [];
    private filesFail: Array<string> = [];
    private filesRequireReset: Array<string> = [];

    private sizeInstalledSinceLastsnapshot = 0;
    private skipErrors = false;

    private installedDatabases = new Map<string, string>();
}

@Injectable({ providedIn: 'root' })
export class InstallationService {
    constructor(
        private cloudpilotService: CloudpilotService,
        private emulationService: EmulationService,
        private loadingController: LoadingController,
        private snapshotService: SnapshotService,
        private alertService: AlertService,
        private fstools: FsToolsService,
    ) {}

    async installFiles(files: Array<FileDescriptor>): Promise<void> {
        const loader = await this.loadingController.create({
            message: 'Installing...',
        });

        await loader.present();
        await this.emulationService.pause();
        await this.snapshotService.waitForPendingSnapshot();

        let filesSuccess: Array<string> = [];
        let filesFail: Array<string> = [];
        let filesRequireReset: Array<string> = [];

        try {
            const installationContext = new InstallationContext(
                this.cloudpilotService,
                this.fstools,
                this.alertService,
                this.snapshotService,
                files,
            );
            [filesSuccess, filesRequireReset, filesFail] = await installationContext.run();
        } finally {
            void loader.dismiss();
            void this.emulationService.resume();
        }

        const message = [
            this.messageSuccess(filesSuccess),
            this.messageFailed(filesFail),
            this.messageRequireReset(filesRequireReset),
        ]
            .filter((x) => !!x)
            .join('<br/><br/>');

        let header;

        if (filesFail.length === 0) {
            header = 'Installation successful';
        } else if (filesSuccess.length === 0) {
            header = 'Installation failed';
        } else {
            header = 'Installation errors';
        }

        void this.alertService.message(
            header,
            message,
            filesRequireReset.length > 0 ? { 'Reset now': () => this.emulationService.reset() } : undefined,
        );
    }

    private messageSuccess(filesSuccess: Array<string>): string | undefined {
        switch (filesSuccess.length) {
            case 0:
                return undefined;

            case 1:
                return `Installation of ${filesSuccess[0]} successful.`;

            default:
                return `Installation of ${filesSuccess.length} items successful.`;
        }
    }

    private messageFailed(filesFail: Array<string>): string | undefined {
        return filesFail.length > 0 ? `Installation of  ${concatFilenames(filesFail)} failed.` : undefined;
    }

    private messageRequireReset(filesRequireReset: Array<string>): string | undefined {
        return filesRequireReset.length > 0
            ? `${concatFilenames(filesRequireReset)} require a device reset.`
            : undefined;
    }
}
