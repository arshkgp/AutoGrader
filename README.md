# AutoGrader

AutoGrader is an automated grading and evaluation system designed to streamline the assessment of student programming submissions. It compiles, executes, and validates code submissions against predefined test suites while tracking runtime metrics, errors, and grading criteria in an isolated environment.

---

## Key Features

* **Automated Code Execution & Testing:** Evaluates candidate source code against standard and edge-case I/O test fixtures.
* **Multi-Language Support:** Configurable runners supporting compilation and execution for C, C++, and Python.
* **Resource & Execution Limits:** Enforces configurable execution time limits (TLE) and memory constraints per test case.
* **Detailed Feedback & Diagnostics:** Captures compilation errors, standard output/error streams, runtime exceptions, and grading summaries.
* **Structured Gradebook Export:** Generates standardized scoring breakdowns and exportable summary reports for instructors.

---

## Architecture Overview

```text
                    ┌────────────────────────┐
                    │   Student Submissions  │
                    └───────────┬────────────┘
                                │
                                ▼
┌──────────────────┐    ┌────────────────────────┐    ┌──────────────────┐
│ Test Case Engine │───▶│ Execution & Validation ├───▶│  Report & Grade  │
│ (Input/Expected) │    │  (Sandbox / Isolates)  │    │    Generator     │
└──────────────────┘    └────────────────────────┘    └──────────────────┘
```

1. **Submission Ingestion:** Scans and organizes student directory hierarchies and source files.
2. **Compilation & Runner Pipeline:** Invokes corresponding toolchains (e.g., `gcc`, `g++`, `python3`) with isolated runtime constraints.
3. **Diff & Verification Engine:** Compares program outputs against reference test cases with whitespace/newline normalization.
4. **Scoring & Aggregation:** Computes weighted test scores, generates CSV/JSON logs, and logs execution anomalies.

---

## Project Structure

```text
AutoGrader/
├── src/
│   ├── core/           # Core grading orchestrator and execution runner
│   ├── compilers/      # Language-specific build and execution handlers
│   ├── utils/          # Diff tools, file parsers, and validation helpers
│   └── main.py         # Entry-point CLI script
├── tests/
│   ├── test_cases/     # Standard inputs and expected outputs
│   └── submissions/    # Sample student submissions for validation
├── config/
│   └── settings.json   # Time limits, score weights, and runner configurations
├── requirements.txt    # Python dependencies
├── .gitignore
└── README.md
```

---

## Getting Started

### Prerequisites

* Python 3.9+
* GCC / G++ (for compiling C/C++ submissions)

### Installation

1. **Clone the repository:**
   ```bash
   git clone https://github.com/arshkgp/AutoGrader.git
   cd AutoGrader
   ```

2. **Set up a virtual environment:**
   ```bash
   python3 -m venv venv
   source venv/bin/activate    # On Windows: venv\Scripts\activate
   ```

3. **Install dependencies:**
   ```bash
   pip install -r requirements.txt
   ```

---

## Configuration & Usage

### 1. Configure Test Cases and Limits
Define runtime configurations, timeouts, and scoring schemes in `config/settings.json`:

```json
{
  "time_limit_sec": 2.0,
  "memory_limit_mb": 256,
  "languages": {
    "cpp": { "compiler": "g++", "flags": ["-O2", "-std=c++17"] },
    "c": { "compiler": "gcc", "flags": ["-O2", "-std=c11"] },
    "python": { "interpreter": "python3" }
  }
}
```

### 2. Run the Grading Engine
Execute the evaluation suite against a directory of submissions:

```bash
python src/main.py --submissions ./path/to/submissions --tests ./path/to/test_cases --output ./grades.csv
```

---

## License

This project is licensed under the MIT License. See the LICENSE file for details.
