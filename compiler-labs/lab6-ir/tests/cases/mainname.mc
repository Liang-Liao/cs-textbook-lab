// main 是顶层段的保留名：用户函数再叫 main 会与顶层段重名，
// irvm 调 main() 时命中顶层段而非函数体（静默错语义）。
int main() {
    return 7;
}
print main();
