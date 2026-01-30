#!/usr/bin/env bash
# Ignored in case of a source call, but needed for bash specific sourcing detection

set -o pipefail

# shellcheck disable=SC2091
if [ -z "${GITHUB_ENV}" ] && ! $(return 0 2>/dev/null); then
  echo "This script must be run by sourcing it:"
  echo "source $0 $*"
  exit 1
fi

realpath() {
    OLDPWD="${PWD}"
    cd "$1" || exit 1
    pwd
    cd "${OLDPWD}" || exit 1
}

# Get script file location, compatible with bash and zsh
if [ -n "$BASH_VERSION" ]; then
  THIS_SCRIPT_NAME="${BASH_SOURCE[0]}"
elif [ -n "$ZSH_VERSION" ]; then
  # shellcheck disable=SC2296
  THIS_SCRIPT_NAME="${(%):-%N}"
else
  THIS_SCRIPT_NAME="$0"
fi

HOST_ARCH=$(uname -m)  # One of x86_64, arm64, i386, ppc or ppc64

# Custom vcpkg artifact repository settings
# Set VCPKG_ARTIFACT_REPO to download from a custom vcpkg repository's GitHub Actions artifacts
# instead of downloads.mixxx.org
[ -z "$VCPKG_ARTIFACT_REPO" ] && VCPKG_ARTIFACT_REPO="mccartyp/vcpkg"
[ -z "$VCPKG_ARTIFACT_RUN_ID" ] && VCPKG_ARTIFACT_RUN_ID="21463852143"

if [ "$HOST_ARCH" = "x86_64" ]; then
	if [ -n "${BUILDENV_ARM64_CROSS}" ]; then
	    if [ -n "${BUILDENV_RELEASE}" ]; then
	        # Using non-release triplet since custom vcpkg artifacts were built without -release
	        VCPKG_TARGET_TRIPLET="arm64-osx-min1100"
	        BUILDENV_BRANCH="2.6"
	        BUILDENV_NAME="mixxx-deps-2.6-arm64-osx-cross-6f8f095"
	        BUILDENV_SHA256="66f5f5c9b53b76c553909c39a4a7e4647dc3e5f0d0e1c91b8546ef3450a92136"
	        BUILDENV_ARTIFACT_ID="5297914431"
	    else
	        VCPKG_TARGET_TRIPLET="arm64-osx-min1100"
	        BUILDENV_BRANCH="2.6"
	        BUILDENV_NAME="mixxx-deps-2.6-arm64-osx-cross-6a4e01e"
	        BUILDENV_SHA256="39ec4249f601e4764b07ba2c8a6b27887c9a8c676d5e8d326035489af967ceac"
	    fi
	else
	    if [ -n "${BUILDENV_RELEASE}" ]; then
	        # Using non-release triplet since custom vcpkg artifacts were built without -release
	        VCPKG_TARGET_TRIPLET="x64-osx-min1100"
	        BUILDENV_BRANCH="2.6"
	        BUILDENV_NAME="mixxx-deps-2.6-x64-osx-6f8f095"
	        BUILDENV_SHA256="d66c586742ba2ffc3742a4da1bd04f8f3f8d92bd4a7fa62b836d7d3832919464"
	        BUILDENV_ARTIFACT_ID="5297914233"
	    else
	        VCPKG_TARGET_TRIPLET="x64-osx-min1100"
	        BUILDENV_BRANCH="2.6"
	        BUILDENV_NAME="mixxx-deps-2.6-x64-osx-6a4e01e"
	        BUILDENV_SHA256="1d12b5f496598fb2d740ac6b5fe540b2256556554b9b6db58846aa14f94c54b2"
	    fi
	fi
elif [ "$HOST_ARCH" = "arm64" ]; then
    if [ -n "${BUILDENV_RELEASE}" ]; then
        VCPKG_TARGET_TRIPLET="arm64-osx-min1100-release"
        BUILDENV_BRANCH="2.6-rel"
        BUILDENV_NAME="mixxx-deps-2.6-arm64-osx-rel-541a925"
        BUILDENV_SHA256="7c160d441802a891092b4e16f7429f32ae7eeed95b4eff5bf2ed403b796b5355"
    else
        VCPKG_TARGET_TRIPLET="arm64-osx-min1100"
        BUILDENV_BRANCH="2.6"
        BUILDENV_NAME="mixxx-deps-2.6-arm64-osx-6a4e01e"
        BUILDENV_SHA256="b883799559b8621af5fa799e7784d1ee0ce9e573e4fed22c846e197eaf5a275b"
    fi
else
    echo "ERROR: Unsupported architecture detected: $HOST_ARCH"
    echo "Please refer to the following guide to manually build the vcpkg environment:"
    echo "https://github.com/mixxxdj/mixxx/wiki/Compiling-dependencies-for-macOS-arm64"
    exit 1
fi

# When using GitHub artifacts, don't set BUILDENV_URL to prevent CMake from
# downloading from mixxx.org. The artifact is pre-downloaded in the workflow.
if [ -n "${VCPKG_ARTIFACT_REPO}" ] && [ -n "${BUILDENV_ARTIFACT_ID}" ]; then
    BUILDENV_URL=""
else
    BUILDENV_URL="https://downloads.mixxx.org/dependencies/${BUILDENV_BRANCH}/macOS/${BUILDENV_NAME}.zip"
fi

MIXXX_ROOT="$(realpath "$(dirname "$THIS_SCRIPT_NAME")/..")"

[ -z "$BUILDENV_BASEPATH" ] && BUILDENV_BASEPATH="${MIXXX_ROOT}/buildenv"

case "$1" in
    name)
        if [ -n "${GITHUB_ENV}" ]; then
            {
                echo "BUILDENV_NAME=$BUILDENV_NAME"
                echo "BUILDENV_SHA256=$BUILDENV_SHA256"
                echo "VCPKG_ARTIFACT_REPO=$VCPKG_ARTIFACT_REPO"
                echo "VCPKG_ARTIFACT_RUN_ID=$VCPKG_ARTIFACT_RUN_ID"
                echo "BUILDENV_ARTIFACT_ID=$BUILDENV_ARTIFACT_ID"
            } >> "${GITHUB_ENV}"
        else
            echo "$BUILDENV_NAME"
        fi
        ;;

    setup)
        BUILDENV_PATH="${BUILDENV_BASEPATH}/${BUILDENV_NAME}"

        export BUILDENV_NAME
        export BUILDENV_BASEPATH
        export BUILDENV_URL
        export BUILDENV_SHA256
        export MIXXX_VCPKG_ROOT="${BUILDENV_PATH}"
        export CMAKE_GENERATOR=Ninja
        export VCPKG_TARGET_TRIPLET="${VCPKG_TARGET_TRIPLET}"
        export VCPKG_ARTIFACT_REPO
        export VCPKG_ARTIFACT_RUN_ID
        export BUILDENV_ARTIFACT_ID

        echo_exported_variables() {
            echo "BUILDENV_NAME=${BUILDENV_NAME}"
            echo "BUILDENV_BASEPATH=${BUILDENV_BASEPATH}"
            echo "BUILDENV_URL=${BUILDENV_URL}"
            echo "BUILDENV_SHA256=${BUILDENV_SHA256}"
            echo "MIXXX_VCPKG_ROOT=${MIXXX_VCPKG_ROOT}"
            echo "CMAKE_GENERATOR=${CMAKE_GENERATOR}"
            echo "VCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}"
            echo "VCPKG_ARTIFACT_REPO=${VCPKG_ARTIFACT_REPO}"
            echo "VCPKG_ARTIFACT_RUN_ID=${VCPKG_ARTIFACT_RUN_ID}"
            echo "BUILDENV_ARTIFACT_ID=${BUILDENV_ARTIFACT_ID}"
        }

        if [ -n "${GITHUB_ENV}" ]; then
            echo_exported_variables >> "${GITHUB_ENV}"
        elif [ "$1" != "--profile" ]; then
            echo ""
            echo "Exported environment variables:"
            echo_exported_variables
            echo "You can now configure cmake from the command line in an EMPTY build directory via:"
            echo "cmake -DCMAKE_TOOLCHAIN_FILE=${MIXXX_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake ${MIXXX_ROOT}"
        fi
        ;;
    *)
        echo "Usage: source macos_buildenv.sh [options]"
        echo ""
        echo "options:"
        echo "   help       Displays this help."
        echo "   name       Displays the name of the required build environment."
        echo "   setup      Setup the build environment variables for download during CMake configuration."
        ;;
esac
