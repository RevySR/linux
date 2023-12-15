#! /bin/bash

set -xe

function do_prep() {
    mkdir -p ${DIST_RPMBUILDDIR_OUTPUT}
    mkdir -p ${DIST_RPMBUILDDIR_OUTPUT}/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

    cp ${DIST_RPM}/cpupower*            ${DIST_RPMBUILDDIR_OUTPUT}/SOURCES/
    cp ${DIST_RPM}/generate_bls_conf.sh ${DIST_RPMBUILDDIR_OUTPUT}/SOURCES/

    # for official build, the corresponding tag should exist
    if [ -n "$DIST_OFFICIAL_BUILD" ]; then
        if ! git tag | grep -q -x "${DIST_PKG_COMMIT_ID}"; then
            echo "cannot find official build tag: ${DIST_PKG_COMMIT_ID}"
            exit 1
        fi
    fi

    pkgname="linux-${DIST_ANOLIS_VERSION}${DIST}"
    pushd ${DIST_SRCROOT} > /dev/null
    git archive --format=tar --prefix="${pkgname}/" ${DIST_PKG_COMMIT_ID} | xz -T$(nproc) > ${DIST_RPMBUILDDIR_OUTPUT}/SOURCES/${pkgname}.tar.xz
    md5sum ${DIST_RPMBUILDDIR_OUTPUT}/SOURCES/${pkgname}.tar.xz > ${DIST_RPMBUILDDIR_OUTPUT}/SOURCES/download
    popd > /dev/null
    DIST_OUTPUT=${DIST_RPMBUILDDIR_OUTPUT}/SPECS/ sh genspec.sh

    cp ${DIST_SRCROOT}/arch/x86/configs/anolis_defconfig		${DIST_RPMBUILDDIR_OUTPUT}/SOURCES/kernel-${DIST_KERNELVERSION}-x86_64.config
    cp ${DIST_SRCROOT}/arch/x86/configs/anolis-debug_defconfig	${DIST_RPMBUILDDIR_OUTPUT}/SOURCES/kernel-${DIST_KERNELVERSION}-x86_64-debug.config
    cp ${DIST_SRCROOT}/arch/arm64/configs/anolis_defconfig		${DIST_RPMBUILDDIR_OUTPUT}/SOURCES/kernel-${DIST_KERNELVERSION}-aarch64.config
    cp ${DIST_SRCROOT}/arch/arm64/configs/anolis-debug_defconfig	${DIST_RPMBUILDDIR_OUTPUT}/SOURCES/kernel-${DIST_KERNELVERSION}-aarch64-debug.config

}

do_prep