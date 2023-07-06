#!/usr/bin/env bash

#     --slave /usr/bin/$1 $1 /usr/bin/$1-\${version} \\

function register_clang_version {
    local version=$1
    local priority=$2

    update-alternatives --remove-all llvm-config || true
    update-alternatives --remove-all llvm-ar || true
    update-alternatives --remove-all llvm-as || true
    update-alternatives --remove-all llvm-bcanalyzer || true
    update-alternatives --remove-all llvm-cov || true
    update-alternatives --remove-all llvm-diff || true
    update-alternatives --remove-all llvm-dis || true
    update-alternatives --remove-all llvm-dwarfdump || true
    update-alternatives --remove-all llvm-extract || true
    update-alternatives --remove-all llvm-link || true
    update-alternatives --remove-all llvm-mc || true
    update-alternatives --remove-all llvm-mcmarkup || true
    update-alternatives --remove-all llvm-nm || true
    update-alternatives --remove-all llvm-objdump || true
    update-alternatives --remove-all llvm-ranlib || true
    update-alternatives --remove-all llvm-readobj || true
    update-alternatives --remove-all llvm-rtdyld || true
    update-alternatives --remove-all llvm-size || true
    update-alternatives --remove-all llvm-stress || true
    update-alternatives --remove-all llvm-symbolizer || true
    update-alternatives --remove-all llvm-tblgen || true
    update-alternatives --remove-all clang || true
    update-alternatives --remove-all clang++ || true
    update-alternatives --remove-all asan_symbolize || true
    update-alternatives --remove-all c-index-test || true
    update-alternatives --remove-all clang-check || true
    update-alternatives --remove-all clang-cl || true
    update-alternatives --remove-all clang-cpp || true
    update-alternatives --remove-all clang-format || true
    update-alternatives --remove-all clang-format-diff || true
    update-alternatives --remove-all clang-import-test || true
    update-alternatives --remove-all clang-include-fixer || true
    update-alternatives --remove-all clang-offload-bundler || true
    update-alternatives --remove-all clang-query || true
    update-alternatives --remove-all clang-rename || true
    update-alternatives --remove-all clang-reorder-fields || true
    update-alternatives --remove-all clang-tidy || true
    update-alternatives --remove-all lldb || true
    update-alternatives --remove-all lldb-server || true

    update-alternatives \
        --install /usr/bin/llvm-config       llvm-config      /usr/bin/llvm-config-${version} ${priority} \
        --slave   /usr/bin/llvm-ar           llvm-ar          /usr/bin/llvm-ar-${version} \
        --slave   /usr/bin/llvm-as           llvm-as          /usr/bin/llvm-as-${version} \
        --slave   /usr/bin/llvm-bcanalyzer   llvm-bcanalyzer  /usr/bin/llvm-bcanalyzer-${version} \
        --slave   /usr/bin/llvm-cov          llvm-cov         /usr/bin/llvm-cov-${version} \
        --slave   /usr/bin/llvm-diff         llvm-diff        /usr/bin/llvm-diff-${version} \
        --slave   /usr/bin/llvm-dis          llvm-dis         /usr/bin/llvm-dis-${version} \
        --slave   /usr/bin/llvm-dwarfdump    llvm-dwarfdump   /usr/bin/llvm-dwarfdump-${version} \
        --slave   /usr/bin/llvm-extract      llvm-extract     /usr/bin/llvm-extract-${version} \
        --slave   /usr/bin/llvm-link         llvm-link        /usr/bin/llvm-link-${version} \
        --slave   /usr/bin/llvm-mc           llvm-mc          /usr/bin/llvm-mc-${version} \
        --slave   /usr/bin/llvm-mcmarkup     llvm-mcmarkup    /usr/bin/llvm-mcmarkup-${version} \
        --slave   /usr/bin/llvm-nm           llvm-nm          /usr/bin/llvm-nm-${version} \
        --slave   /usr/bin/llvm-objdump      llvm-objdump     /usr/bin/llvm-objdump-${version} \
        --slave   /usr/bin/llvm-ranlib       llvm-ranlib      /usr/bin/llvm-ranlib-${version} \
        --slave   /usr/bin/llvm-readobj      llvm-readobj     /usr/bin/llvm-readobj-${version} \
        --slave   /usr/bin/llvm-rtdyld       llvm-rtdyld      /usr/bin/llvm-rtdyld-${version} \
        --slave   /usr/bin/llvm-size         llvm-size        /usr/bin/llvm-size-${version} \
        --slave   /usr/bin/llvm-stress       llvm-stress      /usr/bin/llvm-stress-${version} \
        --slave   /usr/bin/llvm-symbolizer   llvm-symbolizer  /usr/bin/llvm-symbolizer-${version} \
        --slave   /usr/bin/llvm-tblgen       llvm-tblgen      /usr/bin/llvm-tblgen-${version}

    update-alternatives \
        --install /usr/bin/clang                 clang                 /usr/bin/clang-${version} ${priority} \
        --slave   /usr/bin/clang++               clang++               /usr/bin/clang++-${version}  \
        --slave   /usr/bin/asan_symbolize        asan_symbolize        /usr/bin/asan_symbolize-${version} \
        --slave   /usr/bin/c-index-test          c-index-test          /usr/bin/c-index-test-${version} \
        --slave   /usr/bin/clang-check           clang-check           /usr/bin/clang-check-${version} \
        --slave   /usr/bin/clang-cl              clang-cl              /usr/bin/clang-cl-${version} \
        --slave   /usr/bin/clang-cpp             clang-cpp             /usr/bin/clang-cpp-${version} \
        --slave   /usr/bin/clang-format          clang-format          /usr/bin/clang-format-${version} \
        --slave   /usr/bin/clang-format-diff     clang-format-diff     /usr/bin/clang-format-diff-${version} \
        --slave   /usr/bin/clang-import-test     clang-import-test     /usr/bin/clang-import-test-${version} \
        --slave   /usr/bin/clang-include-fixer   clang-include-fixer   /usr/bin/clang-include-fixer-${version} \
        --slave   /usr/bin/clang-offload-bundler clang-offload-bundler /usr/bin/clang-offload-bundler-${version} \
        --slave   /usr/bin/clang-query           clang-query           /usr/bin/clang-query-${version} \
        --slave   /usr/bin/clang-rename          clang-rename          /usr/bin/clang-rename-${version} \
        --slave   /usr/bin/clang-reorder-fields  clang-reorder-fields  /usr/bin/clang-reorder-fields-${version} \
        --slave   /usr/bin/clang-tidy            clang-tidy            /usr/bin/clang-tidy-${version} \
        --slave   /usr/bin/lldb                  lldb                  /usr/bin/lldb-${version} \
        --slave   /usr/bin/lldb-server           lldb-server           /usr/bin/lldb-server-${version}
}

register_clang_version $1 $2
