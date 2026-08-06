# i3e

A lightweight C++ trading-interface experiment focused on speed, simplicity, and portability.

> Built for fun, learning, and the challenge of making a responsive market-data UI without the cost and complexity of professional trading platforms.

## Why i3e?

Many trading platforms are expensive and packed with features that most users never touch. I wanted to explore a different idea:

- A fast native UI written in C++
- A small and understandable codebase
- Support for simple market-data samples
- A project lightweight enough to run even on constrained hardware
- A foundation that could eventually become a practical tool for traders or quantitative research

This is not intended to replace a professional terminal. It is a personal engineering project and a way to learn more about performance-focused software, data handling, UI design, and the type of systems used in trading technology.

## What it does

i3e contains the early building blocks for a lightweight trading application:

- Loading and working with tick/sample market data
- A small engine layer for application logic
- Window-tree management for organizing UI windows
- A native C++ implementation with a focus on low overhead
- A Makefile-based build setup

The repository includes C++ source files for the engine and window tree, a tick-data CSV sample, and a Python helper script for generating tick data. [attached_file:1]

## Project structure

```text
.
├── engine/              # Engine-related source files
├── engine.cpp           # Engine implementation
├── engine.hpp           # Engine interface
├── WindowTree.cpp       # Window-tree implementation
├── WindowTree.hpp       # Window-tree interface
├── main.cpp             # Application entry point
├── ticks.csv            # Sample tick data
├── gen_ticks.py         # Tick-data generator/helper
├── Makefile             # Build configuration
└── i3trade              # Project executable or related artifact
```

## Getting started

### Requirements

You will likely need:

- A C++ compiler with modern C++ support, such as `g++` or `clang++`
- `make`
- Python 3, if you want to generate or modify tick-data samples

### Build

```bash
git clone https://github.com/la-leche/i3e.git
cd i3e
make
```

### Run

After building, run the generated executable:

```bash
./i3trade
```

If the executable name or build target changes, check the `Makefile` for the current command.

## Sample data

The project includes `ticks.csv` as example market data. You can use `gen_ticks.py` to create additional synthetic tick data for testing:

```bash
python3 gen_ticks.py
```

Synthetic data is useful for testing UI responsiveness and data-processing logic without requiring a broker connection, exchange feed, or paid market-data provider.

## Current status

This is an experimental and educational project. It is actively a work in progress rather than production trading software.

Current priorities:

- Keep the application lightweight
- Improve UI responsiveness
- Make the data flow easier to understand
- Test with larger tick-data samples
- Keep the project portable across operating systems
- Learn better C++ architecture and performance practices

## Ideas for the future

Possible directions for i3e include:

- Candlestick and tick charts
- Order-book visualization
- Keyboard-first workflow
- CSV import improvements
- Historical-data replay
- Basic indicators
- Strategy/backtesting experiments
- Broker or exchange API adapters
- Better cross-platform build support
- Performance benchmarks on low-end hardware

## Motivation

I am an Informatics student interested in working as a developer in trading technology or eventually as a quantitative trader.

I made i3e because I wanted a hands-on project that combines:

- C++ development
- Performance-oriented programming
- Financial-market data
- UI architecture
- Practical software engineering

The challenge was simple: **can I make a fast, minimal trading UI that runs on ordinary hardware and works with small data samples, without copying the complexity of expensive platforms?**

## Important note

i3e is a software experiment, not financial advice and not a production-grade trading platform. Do not use it to make real-money trading decisions without independently validating every part of the software and its data.

## Contributing

Feedback, ideas, and improvements are welcome.

If you are interested in C++, trading systems, market data, or lightweight UI design, feel free to open an issue or submit a pull request.

## License

No license has been specified yet. Before using, copying, or distributing this project, please contact the repository owner or add an appropriate open-source license.
