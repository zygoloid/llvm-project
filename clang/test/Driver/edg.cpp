// RUN: %clang -fedg -ccc-print-phases -c %s 2>&1 | FileCheck %s --check-prefix=PHASES
// PHASES: 0: input, "{{.*}}edg.cpp", c++
// PHASES: 1: preprocessor, {0}, c++-cpp-output
// PHASES: 2: compiler, {1}, cpp-output
// PHASES: 3: compiler, {2}, ir
// PHASES: 4: backend, {3}, assembler
// PHASES: 5: assembler, {4}, object

// RUN: %clang -fedg -fno-edg -ccc-print-phases -c %s 2>&1 | FileCheck %s --check-prefix=NO-EDG-PHASES
// NO-EDG-PHASES: 0: input, "{{.*}}edg.cpp", c++
// NO-EDG-PHASES: 1: preprocessor, {0}, c++-cpp-output
// NO-EDG-PHASES: 2: compiler, {1}, ir

// RUN: %clang -### -fedg --target=x86_64-unknown-linux-gnu -std=c++20 \
// RUN:   -DTEST_DEF=42 -UTEST_UNDEF -I/my/include -isystem /my/sysinclude \
// RUN:   -fno-exceptions -fno-rtti -funsigned-char -w \
// RUN:   -Xedg --remarks -Xedg=--display_error_number -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=COMPILE
// COMPILE: "-cc1edg"
// COMPILE-SAME: "-triple" "x86_64-unknown-linux-gnu"
// COMPILE-SAME: "--target=linux_x86_64"
// COMPILE-SAME: "--clang"
// COMPILE-SAME: "--c++20"
// COMPILE-SAME: "--unsigned_chars"
// COMPILE-SAME: "--no_exceptions"
// COMPILE-SAME: "--no_rtti"
// COMPILE-SAME: "--no_warnings"
// COMPILE-SAME: "-DTEST_DEF=42"
// COMPILE-SAME: "-UTEST_UNDEF"
// COMPILE-SAME: "-I/my/include"
// COMPILE-SAME: "--sys_include=/my/sysinclude"
// COMPILE-SAME: "--gen_c_file_name={{.*}}.i"
// COMPILE-SAME: "--remarks"
// COMPILE-SAME: "--display_error_number"
// COMPILE-NEXT: "-cc1"
// COMPILE-SAME: "-std=gnu99"
// COMPILE-SAME: "-w"
// COMPILE-SAME: "-function-alignment" "2"
// COMPILE-SAME: "-x" "cpp-output"

// RUN: %clang -### -fedg --target=aarch64-unknown-linux-gnu -fsyntax-only %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=SYNTAX-ONLY
// SYNTAX-ONLY: "-cc1edg"
// SYNTAX-ONLY-SAME: "-triple" "aarch64-unknown-linux-gnu"
// SYNTAX-ONLY-SAME: "--target=linux_aarch64"
// SYNTAX-ONLY-SAME: "--no_code_gen"
// SYNTAX-ONLY-NOT: "-cc1"

// RUN: %clang -### -fedg -E %s 2>&1 | FileCheck %s --check-prefix=PREPROCESS
// PREPROCESS: "-cc1edg"
// PREPROCESS-SAME: "-E"
// PREPROCESS-NOT: "-cc1"

// RUN: %clang -### -fedg -x c -c %s 2>&1 | FileCheck %s --check-prefix=C-INPUT
// C-INPUT-NOT: "-cc1edg"
// C-INPUT: "-cc1"
