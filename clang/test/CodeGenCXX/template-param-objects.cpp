// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++20 %s -emit-llvm -o - | FileCheck %s

struct S { char buf[32]; };
template<S s> constexpr const char *begin() { return s.buf; }
template<S s> constexpr const char *end() { return s.buf + __builtin_strlen(s.buf); }

// CHECK: [[HELLO:@_ZTAXtl1StlA32_cLc104ELc101ELc108ELc108ELc111ELc32ELc119ELc111ELc114ELc108ELc100EEEE]] = linkonce_odr constant { <{ [11 x i8], [21 x i8] }> } { <{ [11 x i8], [21 x i8] }> <{ [11 x i8] c"hello world", [21 x i8] zeroinitializer }> }, comdat

// CHECK: @p = global i8* getelementptr inbounds ({{.*}}* [[HELLO]], i32 0, i32 0, i32 0, i32 0)
const char *p = begin<S{"hello world"}>();
// CHECK: @q = global i8* getelementptr (i8, i8* getelementptr inbounds ({{.*}}* [[HELLO]], i32 0, i32 0, i32 0, i32 0), i64 11)
const char *q = end<S{"hello world"}>();
