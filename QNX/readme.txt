## Introduction: Automotive ECUs & Basics

1. Vehicle Electrical Architecture
2. E/E Architecture Evolution
3. Functional Domains
4. Operating Systems and ECU Mapping
5. Automotive SoCs

1. Evolution of Vehicle Electrical Architecture
   - Distributed Architecture (Each function has its own ECU)
   - Domain Architecture
   - Zonal Architecture
   - SDV (Software Derived Vehicle)

3. Realtime Concepts (Done)
   - What is RTOS?
   - Hard vs soft real time

4. The QNX OS Microkernel (Done)
   - IPC
   - Interrupt Redirector
   - Scheduler 

5. Interprocess Communication (IPC) (Done)
   - Synchronous message passing (MsgSend/MsgReceive/MsgReply & Channels and Connections & Flags for ChannelCreate())
   - MultiPart Messages Using IOV (MsgSendv())
   - Pulses(Code, Value)
   - Signals
   - Event (struct sigevent event)
   - Shared Memory
   - Pipes

6. Process Manager (Done)
   - Process Management
   - Memory management and protected address spaces
   - Pathname management and namespace

7. Thread & Process Scheduling
   - Scheduling algorithms (FIFO, round-robin, sporadic)
   - Thread APIS (Kernel Functions, POSIX Functions, C11 Thread function)
   - Thread Synchronisation (Mutexes, Condition variables (pthread_cond_t), Semaphores, Read-write locks, Barriers, Sleepon Locks)
   - Cluster Based Scheduling (Core Affinity => We can change which cluster a thread belongs to)
   - Priority inheritance and priority inversion handling
   - Adaptive Partitioning Scheduler (APS)

8. Resource Managers
   - Role of resource managers (user-written drivers)
   - Device Drivers
   - Device abstraction and the /dev namespace
   - Writing and registering a resource manager

9. Dynamic Linking
   - Shared libraries and shared objects
   - Runtime linking behavior

10. Interrupt Handling
    - ISR
    - IST
    - IPI

11. Boot Sequence
    - Power-on, IPL, and bootloader stages
    - Startup program: hardware init and kernel handoff
    - Microkernel and Process Manager (procnto) initialization
    - Mounting the boot image filesystem
    - Executing buildfile scripts and starting system processes
    - Board Support Packages (BSPs)

12. Timing Architecture
    - QNX Time representation (CLOCK_MONOTONIC & CLOCK_REALTIME)
    - TIMERS
    - High Resolution Timers

13. Filesystems
    - Power-Safe filesystem
    - Image filesystem, DOS, Ext2, QNX6
    - QNX Filesystem for Safety (QFS)

14. Character I/O
    - Serial and parallel device handling
    - High-performance character I/O design

15. Networking Architecture
    - Unified networking interface (io-net/io-sock)
    - TCP/IP networking (io-sock, FreeBSD-based stack)
    - High-Performance Networking Stack