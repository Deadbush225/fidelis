# How to Run Fidelis

Follow these steps in **Windows PowerShell**.

## 1. Open the project folder

```powershell
cd "C:\Users\lenovo\Desktop\fidelis"
```
Or, use whatever the path you put this folder on.

## 2. Check GCC

```powershell
gcc --version
```

If PowerShell says that `gcc` is not recognized, install GCC through MinGW-w64 or MSYS2 first.

## 3. Build the program

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -O2 -Iinclude src\main.c src\lexer.c -o fidelis.exe
```

## 4. Run the default sample

```powershell
.\fidelis.exe
```

## 5. Run a specific sample

```powershell
.\fidelis.exe samples\all_features.swift
```

Other available samples are:

```text
samples\test_features.swift
samples\escape_test.swift
```

## 6. Clean the generated executable

```powershell
Remove-Item fidelis.exe -ErrorAction SilentlyContinue
```
