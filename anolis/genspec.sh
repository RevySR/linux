#! /bin/bash
# generate kernel spec through spec template and changelog files.
# it it call from Makefile, do not run it directly.

mkdir -p ${DIST_OUTPUT}
cp -f ${DIST_RPM}/${DIST_SPEC_TEMPLATE} ${DIST_OUTPUT}/${DIST_SPEC_FILE}

for changelog_file in $(ls ${DIST_CHANGELOG} | sort)
do
    sed -i "/%changelog/r ${DIST_CHANGELOG}/${changelog_file}" ${DIST_OUTPUT}/${DIST_SPEC_FILE}
done

sed -i -e "
    s/%%DIST%%/$DIST/
    s/%%DIST_KERNELVERSION%%/$DIST_KERNELVERSION/
    s/%%DIST_PKGRELEASEVERION%%/$DIST_PKGRELEASEVERION/" ${DIST_OUTPUT}/${DIST_SPEC_FILE}

function generate_cmdline() {
    local arch=$1
    local cmdline=""
    for cmd in $(awk '!/^#/ && !/^[[:space:]]*$/' ${DIST_SOURCES}cmdline/${arch})
    do
        cmdline="${cmdline} ${cmd}"
    done
    echo "${cmdline}"
}

x86_cmdline=$(generate_cmdline x86)
arm_cmdline=$(generate_cmdline arm64)
loongarch_cmdline=$(generate_cmdline loongarch64)
riscv_cmdline=$(generate_cmdline riscv)
sed -i -e "s/%%X86_CMDLINE%%/$x86_cmdline/" ${DIST_OUTPUT}/${DIST_SPEC_FILE}
sed -i -e "s/%%ARM_CMDLINE%%/$arm_cmdline/" ${DIST_OUTPUT}/${DIST_SPEC_FILE}
sed -i -e "s/%%LOONGARCH_CMDLINE%%/$loongarch_cmdline/" ${DIST_OUTPUT}/${DIST_SPEC_FILE}
sed -i -e "s/%%RISCV_CMDLINE%%/$riscv_cmdline/" ${DIST_OUTPUT}/${DIST_SPEC_FILE}
