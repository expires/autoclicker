# Build-time generator for Revision.h. Invoked via `cmake -P` from a custom
# target so it re-runs on every build (current HEAD SHA + dirty flag).
#
# The header is only rewritten when the resulting content actually changes,
# so no-op builds stay no-op — the dependent .cpp doesn't get re-compiled
# unless the commit moved or the working tree's dirty state flipped.
#
# Required cache vars (passed via -D from the caller):
#   SOURCE_DIR : repo root (where .git/ lives)
#   OUTPUT     : full path to write Revision.h to

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${SOURCE_DIR}
    OUTPUT_VARIABLE GIT_REV
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE GIT_REV_RC
    ERROR_QUIET
)
if(NOT GIT_REV_RC EQUAL 0 OR NOT GIT_REV)
    set(GIT_REV "unknown")
endif()

# Append -dirty if there are uncommitted changes. Catches untracked files
# too; the webhook then makes it visible that the build came from a
# non-committed working tree, not a clean revision.
execute_process(
    COMMAND git status --porcelain
    WORKING_DIRECTORY ${SOURCE_DIR}
    OUTPUT_VARIABLE GIT_STATUS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(GIT_STATUS)
    set(GIT_REV "${GIT_REV}-dirty")
endif()

set(_content "#pragma once\n#define BUILD_REVISION \"${GIT_REV}\"\n")

# Idempotent write: skip if existing content matches. Without this, the
# custom target marks Revision.h "out of date" on every build and the
# downstream .cpp recompiles even when SHA hasn't moved.
if(EXISTS ${OUTPUT})
    file(READ ${OUTPUT} _existing)
    if(_existing STREQUAL _content)
        return()
    endif()
endif()
file(WRITE ${OUTPUT} "${_content}")
