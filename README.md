# Intelligent Emergency Evacuation Planning System (EES)

This project is a C++17 evacuation simulator that:

- models rooms, junctions, exits, and corridors as a graph,
- computes shortest room-to-exit routes,
- assigns evacuees to exits,
- runs a time-based evacuation simulation with corridor capacity limits,
- writes a detailed report to an output text file.

## 1. Prerequisites

You need:

- Windows PowerShell
- `g++` compiler with C++17 support (already validated in this environment)

To verify `g++` is available:

```powershell
g++ --version
```

If this prints a version, you are ready to build.

## 2. Project Files

Main source files used for build:

- `main.cpp`
- `graph.cpp`
- `dijkstra.cpp`
- `assignment.cpp`
- `scheduler.cpp`

Input scenarios provided:

- `input.txt`
- `easy_input.txt`
- `medium_input.txt`
- `hard_input.txt`
- `scenario_input.txt`

Expected output examples provided:

- `easy_output.txt`
- `medium_output.txt`
- `hard_output.txt`
- `output.txt`

## 3. Build Instructions (Windows PowerShell)

Open PowerShell in the project root (the folder that contains `main.cpp`) and run:

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -pedantic main.cpp graph.cpp dijkstra.cpp assignment.cpp scheduler.cpp -o ees.exe
```

This creates `ees.exe` in the same folder.

## 4. Run Instructions

### Option A: Run with default input and output

```powershell
.\ees.exe
```

Default behavior from `main.cpp`:

- input file: `input.txt`
- output file: `output.txt`

### Option B: Run with custom input and output

```powershell
.\ees.exe <input_file> <output_file>
```

Example:

```powershell
.\ees.exe easy_input.txt easy_output_generated.txt
```

## 5. Recommended Test Runs

Run each provided scenario:

```powershell
.\ees.exe easy_input.txt easy_output_generated.txt
.\ees.exe medium_input.txt medium_output_generated.txt
.\ees.exe hard_input.txt hard_output_generated.txt
.\ees.exe scenario_input.txt scenario_output_generated.txt
```

## 6. View Output Report

After running, open an output file in terminal or editor.

PowerShell preview command:

```powershell
Get-Content easy_output_generated.txt -TotalCount 40
```

The report includes:

- scenario summary,
- validation results,
- route table,
- assignment table,
- timeline log,
- final metrics and status.

## 7. Clean and Rebuild

If you want a fresh rebuild:

```powershell
Remove-Item .\ees.exe -ErrorAction SilentlyContinue
g++ -std=c++17 -O2 -Wall -Wextra -pedantic main.cpp graph.cpp dijkstra.cpp assignment.cpp scheduler.cpp -o ees.exe
```

## 8. Exit Codes

`ees.exe` returns:

- `0` for successful run (valid scenario)
- `1` when input loading fails, validation fails, or an unhandled runtime error occurs

## 9. Common Issues

### `g++` is not recognized

Your compiler is not on `PATH`. Install MinGW/MSYS2 and reopen PowerShell, or add compiler `bin` directory to `PATH`.

### Failed to load scenario input

Make sure:

- you are running command from project root, and
- input filename is correct.

### No output file appears

Check if the program printed an error to terminal. If input parsing fails, an output file may still be created with the failure reason.

## 10. One-Command Example

Compile and run quickly in one PowerShell line:

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -pedantic main.cpp graph.cpp dijkstra.cpp assignment.cpp scheduler.cpp -o ees.exe; .\ees.exe easy_input.txt easy_output_generated.txt
```

---

If you want, this README can be extended with a section describing the exact scenario file format accepted by the parser in this codebase.