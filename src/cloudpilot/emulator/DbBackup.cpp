#include "DbBackup.h"

#include "EmSystemState.h"
#include "zip.h"

namespace {
    constexpr int COMPRESSION_LEVEL = 1;

    string Suffix(DatabaseInfo& dbInfo) {
        if (dbInfo.creator == sysFileCClipper && (dbInfo.dbAttrs & dmHdrAttrLaunchableData))
            return ".pqa";
        if (dbInfo.dbAttrs & dmHdrAttrResDB) return ".prc";

        return ".pdb";
    }
}  // namespace

DbBackup::~DbBackup() {
    if (zip) zip_stream_close(zip);
    if (archive) free(archive);
}

bool DbBackup::Init(bool includeRomDatabases) {
    EmAssert(state == State::created);

    if (!GetDatabases(databases, includeRomDatabases ? GetDatabaseFlags::kDatabaseFlagsNone
                                                     : GetDatabaseFlags::kOnlyRamDatabases)) {
        return false;
    }

    currentDb = databases.begin();

    state = currentDb == databases.end() ? State::done : State::inProgress;

    zip = zip_stream_open(nullptr, 0, COMPRESSION_LEVEL, 'w');
    exporter = make_unique<DbExporter>(zip);

    return true;
}

bool DbBackup::IsInProgress() const { return state == State::inProgress; }

bool DbBackup::IsDone() const { return state == State::done; }

const char* DbBackup::GetCurrentDatabase() {
    EmAssert(state == State::inProgress);
    EmAssert(currentDb != databases.end());

    currentDatabase = string(currentDb->dbName) + Suffix(*currentDb);

    return currentDatabase.c_str();
}

bool DbBackup::Save() {
    EmAssert(state == State::inProgress);
    EmAssert(currentDb != databases.end());
    EmAssert(zip);

    zip_entry_open(zip, GetCurrentDatabase());

    bool success = exporter->Export(*currentDb);

    zip_entry_close(zip);

    currentDb++;
    if (currentDb == databases.end()) state = State::done;

    return success;
}

void DbBackup::Skip() {
    EmAssert(state == State::inProgress);
    EmAssert(currentDb != databases.end());

    currentDb++;
    if (currentDb == databases.end()) state = State::done;
}

pair<ssize_t, uint8*> DbBackup::GetArchive() {
    if (!archive) zip_stream_copy(zip, (void**)&archive, &archiveSize);

    return pair(archiveSize, archive);
}

uint8* DbBackup::GetArchivePtr() { return GetArchive().second; }

ssize_t DbBackup::GetArchiveSize() { return GetArchive().first; }
