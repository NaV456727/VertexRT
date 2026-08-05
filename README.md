# VertexRT

VertexRT is a lightweight Real-Time Operating System (RTOS) being developed from scratch for learning and understanding embedded operating system internals.

The goal of this project is to explore how an RTOS works by implementing its core components step by step, inspired by production RTOSes such as FreeRTOS while keeping the code clean, modular, and well documented.

## Current Features

- Task Control Blocks (TCB)
- Doubly Linked List implementation
- Round-Robin Scheduler
- Priority-based Ready Queue
- Task Creation and Initialization
- Basic Tick Counter
- Yield API
- ESP32 Port Layer (Initial Stack Initialization)

## Planned Features

- Context Switching
- Assembly Port for ESP32
- Preemptive Scheduling
- Delay Lists
- Semaphores
- Mutexes
- Queues
- Software Timers
- Event Groups
- Memory Management

## Project Structure

```
include/
src/
├── kernel/        # Kernel components
├── arch/esp32/    # ESP32-specific port
docs/              # Documentation
```

## Status

🚧 Work in Progress

The project is under active development and is intended as an educational implementation of an RTOS rather than a production-ready kernel.
