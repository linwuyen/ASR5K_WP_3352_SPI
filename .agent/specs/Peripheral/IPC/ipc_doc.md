The Interprocessor Communications (IPC) module allows communication between the two CPU subsystems.
7.1 Introduction...............................................................................................................................................................917
7.2 Message RAMs......................................................................................................................................................... 918
7.3 IPC Flags and Interrupts..........................................................................................................................................918
7.4 IPC Command Registers..........................................................................................................................................918
7.5 Free-Running Counter..............................................................................................................................................918
7.6 IPC Communication Protocol..................................................................................................................................919
7.7 IPC Registers............................................................................................................................................................ 920
Chapter 7 
Interprocessor Communication (IPC)
Interprocessor Communication (IPC)
www.ti.com
916
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
7.1 Introduction
This section details the IPC features that each CPU can use to request and share information. The IPC features 
are:
•
Message RAMs
•
IPC flags and interrupts
•
IPC command registers
•
Flash pump semaphore
•
Clock configuration semaphore
•
Free-running counter
All IPC features are independent of each other, and most do not require any specific data format.
There are also two registers for boot mode and status communication. Please refer to the boot ROM chapter for 
more information on these registers.
Figure 7-1 shows the design structure of the IPC module.
CPU2
Gen Int Pulse
(on FLG 0->1)
Gen Int Pulse
(on FLG 0->1)
SET31
CLR31
FLG31
ACK31
SET0
CLR0
FLG0
ACK0
SET31
CLR31
FLG31
ACK31
SET0
CLR0
FLG0
ACK0
CPU2.EmulationHalt
PLLSYSCLK
R=0/W=1
C1TOC2IPCINT1/2/3/4
R=0/W=1
R=0/W=1
R
R
R
R
R
IPCSET[31:0]
IPCCLR[31:0]
R
R
R
R
R
IPCACK[31:0]
R
IPCSENDCOM[31:0]
IPCRECVCOM[31:0]
IPCRECVCOM[31:0]
IPCRECVADDR[31:0]
IPCRECVADDR[31:0]
IPCRECVDATA[31:0]
IPCRECVDATA[31:0]
IPCLOCALREPLY[31:0]
IPCLOCALREPLY[31:0]
IPCSENDADDR[31:0]
IPCSENDADDR[31:0]
IPCSENDDATA[31:0]
IPCSENDDATA[31:0]
R/W
R/W
R/W
R/W
R/W
R/W
R/W
R/W
R
64-bit Free Run Counter
IPCCOUNTERH/L[31:0]
C1TOC2IPCCOM[31:0]
C1TOC2IPCADDR[31:0]
C1TOC2IPCDATAW[31:0]
C1TOC2IPCDATAR[31:0]
R=0/W=1
IPCACK[31:0]
IPCSET[31:0]
IPCCLR[31:0]
R
IPCFLG[31:0]
R=0/W=1
R=0/W=1
R
IPCSTS[31:0]
C2TOC1IPCCOM[31:0]
C2TOC1IPCADDR[31:0]
C2TOC1IPCDATAW[31:0]
C2TOC1IPCDATAR[31:0]
CPU1
C2TOC1IPCINT1/2/3/4
R
R/W
R/W
R
IPCBOOTMODE[31:0]
IPCBOOTSTS[31:0]
CPU1.EmulationHalt
IPCFLG[31:0]
IPCREMOTEREPLY[31:0]
IPCREMOTEREPLY[31:0]
IPCSTS[31:0]
IPCSENDCOM[31:0]
CPU2.
ePIE
CPU1.
ePIE
Figure 7-1. IPC Module Architecture
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
917
Copyright © 2024 Texas Instruments Incorporated
7.2 Message RAMs
There are two dedicated 2-kB blocks of message RAM. Each CPU and the DMA have read and write access to 
one RAM and read-only access to the other RAM, as shown in Table 7-1..
Reading or writing a message RAM does not trigger any events on the remote CPU.
Table 7-1. IPC Message RAM Read/Write Access
CPU1
CPU2
CPU1 DMA
CPU2 DMA
CPU1 to CPU2 (1K x 16, address 0x03FC00)
R/W
R
R/W
R
CPU2 to CPU1 (1K x 16, address 0x03F800)
R
R/W
R
R/W
7.3 IPC Flags and Interrupts
There are 32 IPC event signals in each direction between the CPU pairs. These signals can be used for 
flag-based event polling. With the C28x core, four of them (IPC0 - IPC3) can be configured to generate IPC 
interrupts on the remote CPU.
7.4 IPC Command Registers
The IPC command registers provide a simple and flexible way for the CPUs to exchange more complex 
messages. Each CPU has eight dedicated registers; four for sending messages and four for receiving 
messages. The register names were chosen to support a simple command/response protocol, but can be 
used for any purpose. Only the read/write permissions are determined by hardware; the data format is entirely 
software-defined.
For sending messages, each CPU has three writable registers and one read-only register. Those same registers 
are accessible on the remote CPU as three read-only registers and one writable register. Table 7-2 shows the 
command registers.
Table 7-2. IPC Command Registers
Local Register Name
Local CPU
Remote CPU
Remote Register Name
IPCSENDCOM
R/W
R
IPCRECVCOM
IPCSENDADDR
R/W
R
IPCRECVADDR
IPCSENDDATA
R/W
R
IPCRECVDATA
IPCREMOTEREPLY
R
R/W
IPCLOCALREPLY
 
7.5 Free-Running Counter
A 64-bit free-running counter is present in the device and can be used to timestamp IPC events between 
processors. The counter is clocked by PLLSYSCLK and reset by SYSRSn. The counter is implemented 
as two 32-bit registers, IPCCOUNTERH and IPCCOUNTERL. When IPCCOUNTERL is read, the value of 
IPCCOUNTERH is saved. A subsequent read to IPCCOUNTERH returns this saved value. Therefore, the user 
must always read IPCCOUNTERL first then read IPCCOUNTERH next. This design prevents race conditions 
due to IPCCOUNTERL overflowing between reads of the two registers.
The free-running counter stops only when emulation is suspended (when debugger hits a breakpoint) on all 
CPUs. If any core is executing, the counter runs.
Interprocessor Communication (IPC)
www.ti.com
918
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
7.6 IPC Communication Protocol
This section describes the hardware support options for IPC communication between the two CPUs. These 
options can be used independently or in combination. All flag definitions and data formats are entirely user-
defined.
•
The flag system supports event-based communication via interrupts and register polling.
–
CPUx can raise an IPC event by writing to any of the 32 bits of the IPCSET register. This sets the 
corresponding bits in the CPUx IPCFLG register and CPUy IPCSTS register.
–
CPUy can signal the response to the event by setting the appropriate bit in the IPCACK register. This 
clears the corresponding bits in the CPUx IPCFLG register and the CPUy IPCSTS register.
–
If CPUx needs to cancel an event, CPUx can set the appropriate bit in the IPCCLR register. This has the 
same effect as CPUy writing to IPCACK.
–
Flags 0–3 (set using IPCSET[3:0]) fire interrupts to the remote CPU. The remote CPU must configure the 
ePIE module properly to receive an IPC interrupt. Flags 4–31 (set using IPCSET[31:4]) do not produce 
interrupts. Multiple flags can be set, acknowledged, and cleared simultaneously.
•
The command registers support sending several distinct pieces of information and are named COM, ADDR, 
DATA, and REPLY for convenience only and can hold whatever data the application needs.
–
CPUx can write data to the IPCSENDCOM, IPCSENDADDR, and IPCSENDDATA registers. CPUy 
receives these in the IPCRECVCOM, IPCRECVADDR, and IPCRECVDATA registers.
–
CPUy can respond by writing to its IPCLOCALREPLY register. CPUx receives this data in its 
ownIPCREMOTEREPLY register.
•
There is an additional pair of command-like registers offered for boot-time IPC or any other convenient 
use — IPCBOOTMODE and IPCBOOTSTS. Both CPUs can read these registers. CPUx can only write to 
IPCBOOTMODE, and CPUy can only write to IPCBOOTSTS.
•
There are two shared memories for passing large amounts of data between the CPUs. Each CPU has a 
writable memory for sending data and a read-only memory for receiving data.
•
Here is an example of how to use these features together. CPUx needs some data from CPUy's LS RAM. 
The data is at CPUy address 0x9400 and is 0x80 16-bit words long. The protocol can be implemented like 
this:
–
CPUx writes 0x1 to IPCSENDCOM, defined in software to mean "copy data from address". CPUx writes 
the address (0x9400) to IPCSENDADDR and the data length (0x80) to IPCSENDDATA.
–
CPUx writes to IPCSET[3] and IPCSET[16]. Here, IPC flag 3 is configured to send an interrupt and 
IPCSET[16] is defined in software to indicate an incoming command. CPUx begins polling for IPCFLG[3] 
to go low.
–
CPUy receives the interrupt. In the interrupt handler, CPUy checks IPCSTS, finds that flag 16 is set, and 
runs a command processor.
–
CPUy reads the command (0x1) from IPCRECVCOM, the address (0x9400) from IPCRECVADDR, and 
the data length (0x80) from IPCRECVDATA. CPUy then copies the LS RAM data to an empty space in the 
writable shared memory starting at offset 0x210.
–
CPUy writes the shared memory address (0x210) to the IPCLOCALREPLY register. CPUy then writes to 
IPCACK[16] and IPCACK[3] to clear the flags and indicate completion of the command. CPUy's work is 
done.
–
CPUx sees IPCFLG[3] go low. CPUx reads IPCREMOTEREPLY to get the shared memory offset of the 
copied data (0x210).
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
919
Copyright © 2024 Texas Instruments Incorporated
7.7 IPC Registers
This section describes the Interprocessor Communication Registers.
7.7.1 IPC Base Addresses
Table 7-3. IPC Base Addresses
Device Registers
Register Name
Start Address
End Address
IpcRegs (CPU1)
IPC_REGS_CPU1
0x0005_0000
0x0005_0023
IpcRegs (CPU2)
IPC_REGS_CPU2
0x0005_0000
0x0005_0023
Interprocessor Communication (IPC)
www.ti.com
920
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
7.7.2 IPC_REGS_CPU1 Registers
Table 7-4 lists the memory-mapped registers for the IPC_REGS_CPU1 registers. All register offset addresses 
not listed in Table 7-4 should be considered as reserved locations and the register contents should not be 
modified.
Table 7-4. IPC_REGS_CPU1 Registers
Offset
Acronym
Register Name
Write Protection
Section
0h
IPCACK
IPC incoming flag clear (acknowledge) register
Go
2h
IPCSTS
IPC incoming flag status register
Go
4h
IPCSET
IPC remote flag set register
Go
6h
IPCCLR
IPC remote flag clear register
Go
8h
IPCFLG
IPC remote flag status register
Go
Ch
IPCCOUNTERL
IPC Counter Low Register
Go
Eh
IPCCOUNTERH
IPC Counter High Register
Go
10h
IPCSENDCOM
Local to Remote IPC Command Register
Go
12h
IPCSENDADDR
Local to Remote IPC Address Register
Go
14h
IPCSENDDATA
Local to Remote IPC Data Register
Go
16h
IPCREMOTEREPLY
Remote to Local IPC Reply Data Register
Go
18h
IPCRECVCOM
Remote to Local IPC Command Register
Go
1Ah
IPCRECVADDR
Remote to Local IPC Address Register
Go
1Ch
IPCRECVDATA
Remote to Local IPC Data Register
Go
1Eh
IPCLOCALREPLY
Local to Remote IPC Reply Data Register
Go
20h
IPCBOOTSTS
CPU2 to CPU1 IPC Boot Status Register
Go
22h
IPCBOOTMODE
CPU1 to CPU2 IPC Boot Mode Register
Go
Complex bit access types are encoded to fit into small table cells. Table 7-5 shows the codes that are used for 
access types in this section.
Table 7-5. IPC_REGS_CPU1 Access Type Codes
Access Type
Code
Description
Read Type
R
R
Read
R-0
R
-0
Read
Returns 0s
Write Type
W
W
Write
W1S
W
1S
Write
1 to set
Reset or Default Value
-n
Value after reset or the default value
Register Array Variables
i,j,k,l,m,n
When these variables are used in a register name, 
an offset, or an address, they refer to the value of a 
register array where the register is part of a group 
of repeating registers. The register groups form a 
hierarchical structure and the array is represented 
with a formula.
y
When this variable is used in a register name, an 
offset, or an address it refers to the value of a 
register array.
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
921
Copyright © 2024 Texas Instruments Incorporated
7.7.2.1 IPCACK Register (Offset = 0h) [Reset = 00000000h] 
IPCACK is shown in Figure 7-2 and described in Table 7-6.
Return to the Summary Table.
IPC incoming flag clear (acknowledge) register
Figure 7-2. IPCACK Register
31
30
29
28
27
26
25
24
IPC31
IPC30
IPC29
IPC28
IPC27
IPC26
IPC25
IPC24
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
23
22
21
20
19
18
17
16
IPC23
IPC22
IPC21
IPC20
IPC19
IPC18
IPC17
IPC16
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
15
14
13
12
11
10
9
8
IPC15
IPC14
IPC13
IPC12
IPC11
IPC10
IPC9
IPC8
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
7
6
5
4
3
2
1
0
IPC7
IPC6
IPC5
IPC4
IPC3
IPC2
IPC1
IPC0
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
Table 7-6. IPCACK Register Field Descriptions
Bit
Field
Type
Reset
Description
31
IPC31
R-0/W1S
0h
Writing 1 to this bit clears the IPC31 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
30
IPC30
R-0/W1S
0h
Writing 1 to this bit clears the IPC30 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
29
IPC29
R-0/W1S
0h
Writing 1 to this bit clears the IPC29 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
28
IPC28
R-0/W1S
0h
Writing 1 to this bit clears the IPC28 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
27
IPC27
R-0/W1S
0h
Writing 1 to this bit clears the IPC27 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
26
IPC26
R-0/W1S
0h
Writing 1 to this bit clears the IPC26 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
25
IPC25
R-0/W1S
0h
Writing 1 to this bit clears the IPC25 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
922
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
Table 7-6. IPCACK Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
24
IPC24
R-0/W1S
0h
Writing 1 to this bit clears the IPC24 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
23
IPC23
R-0/W1S
0h
Writing 1 to this bit clears the IPC23 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
22
IPC22
R-0/W1S
0h
Writing 1 to this bit clears the IPC22 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
21
IPC21
R-0/W1S
0h
Writing 1 to this bit clears the IPC21 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
20
IPC20
R-0/W1S
0h
Writing 1 to this bit clears the IPC20 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
19
IPC19
R-0/W1S
0h
Writing 1 to this bit clears the IPC19 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
18
IPC18
R-0/W1S
0h
Writing 1 to this bit clears the IPC18 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
17
IPC17
R-0/W1S
0h
Writing 1 to this bit clears the IPC17 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
16
IPC16
R-0/W1S
0h
Writing 1 to this bit clears the IPC16 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
15
IPC15
R-0/W1S
0h
Writing 1 to this bit clears the IPC15 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
14
IPC14
R-0/W1S
0h
Writing 1 to this bit clears the IPC14 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
13
IPC13
R-0/W1S
0h
Writing 1 to this bit clears the IPC13 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
12
IPC12
R-0/W1S
0h
Writing 1 to this bit clears the IPC12 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
11
IPC11
R-0/W1S
0h
Writing 1 to this bit clears the IPC11 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
923
Copyright © 2024 Texas Instruments Incorporated
Table 7-6. IPCACK Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
10
IPC10
R-0/W1S
0h
Writing 1 to this bit clears the IPC10 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
9
IPC9
R-0/W1S
0h
Writing 1 to this bit clears the IPC9 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
8
IPC8
R-0/W1S
0h
Writing 1 to this bit clears the IPC8 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
7
IPC7
R-0/W1S
0h
Writing 1 to this bit clears the IPC7 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
6
IPC6
R-0/W1S
0h
Writing 1 to this bit clears the IPC6 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
5
IPC5
R-0/W1S
0h
Writing 1 to this bit clears the IPC5 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
4
IPC4
R-0/W1S
0h
Writing 1 to this bit clears the IPC4 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
3
IPC3
R-0/W1S
0h
Writing 1 to this bit clears the IPC3 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
2
IPC2
R-0/W1S
0h
Writing 1 to this bit clears the IPC2 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
1
IPC1
R-0/W1S
0h
Writing 1 to this bit clears the IPC1 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
0
IPC0
R-0/W1S
0h
Writing 1 to this bit clears the IPC0 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
924
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
7.7.2.2 IPCSTS Register (Offset = 2h) [Reset = 00000000h] 
IPCSTS is shown in Figure 7-3 and described in Table 7-7.
Return to the Summary Table.
IPC incoming flag status register
Figure 7-3. IPCSTS Register
31
30
29
28
27
26
25
24
IPC31
IPC30
IPC29
IPC28
IPC27
IPC26
IPC25
IPC24
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
23
22
21
20
19
18
17
16
IPC23
IPC22
IPC21
IPC20
IPC19
IPC18
IPC17
IPC16
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
15
14
13
12
11
10
9
8
IPC15
IPC14
IPC13
IPC12
IPC11
IPC10
IPC9
IPC8
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
7
6
5
4
3
2
1
0
IPC7
IPC6
IPC5
IPC4
IPC3
IPC2
IPC1
IPC0
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
Table 7-7. IPCSTS Register Field Descriptions
Bit
Field
Type
Reset
Description
31
IPC31
R
0h
Indicates to the local CPU if the IPC31 event flag was set by the 
remote CPU.
0: No IPC31 event was set by the remote CPU
1: An IPC31 event was set by the remote CPU
Reset type: SYSRSn
30
IPC30
R
0h
Indicates to the local CPU if the IPC30 event flag was set by the 
remote CPU.
0: No IPC30 event was set by the remote CPU
1: An IPC30 event was set by the remote CPU
Reset type: SYSRSn
29
IPC29
R
0h
Indicates to the local CPU if the IPC29 event flag was set by the 
remote CPU.
0: No IPC29 event was set by the remote CPU
1: An IPC29 event was set by the remote CPU
Reset type: SYSRSn
28
IPC28
R
0h
Indicates to the local CPU if the IPC28 event flag was set by the 
remote CPU.
0: No IPC28 event was set by the remote CPU
1: An IPC28 event was set by the remote CPU
Reset type: SYSRSn
27
IPC27
R
0h
Indicates to the local CPU if the IPC27 event flag was set by the 
remote CPU.
0: No IPC27 event was set by the remote CPU
1: An IPC27 event was set by the remote CPU
Reset type: SYSRSn
26
IPC26
R
0h
Indicates to the local CPU if the IPC26 event flag was set by the 
remote CPU.
0: No IPC26 event was set by the remote CPU
1: An IPC26 event was set by the remote CPU
Reset type: SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
925
Copyright © 2024 Texas Instruments Incorporated
Table 7-7. IPCSTS Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
25
IPC25
R
0h
Indicates to the local CPU if the IPC25 event flag was set by the 
remote CPU.
0: No IPC25 event was set by the remote CPU
1: An IPC25 event was set by the remote CPU
Reset type: SYSRSn
24
IPC24
R
0h
Indicates to the local CPU if the IPC24 event flag was set by the 
remote CPU.
0: No IPC24 event was set by the remote CPU
1: An IPC24 event was set by the remote CPU
Reset type: SYSRSn
23
IPC23
R
0h
Indicates to the local CPU if the IPC23 event flag was set by the 
remote CPU.
0: No IPC23 event was set by the remote CPU
1: An IPC23 event was set by the remote CPU
Reset type: SYSRSn
22
IPC22
R
0h
Indicates to the local CPU if the IPC22 event flag was set by the 
remote CPU.
0: No IPC22 event was set by the remote CPU
1: An IPC22 event was set by the remote CPU
Reset type: SYSRSn
21
IPC21
R
0h
Indicates to the local CPU if the IPC21 event flag was set by the 
remote CPU.
0: No IPC21 event was set by the remote CPU
1: An IPC21 event was set by the remote CPU
Reset type: SYSRSn
20
IPC20
R
0h
Indicates to the local CPU if the IPC20 event flag was set by the 
remote CPU.
0: No IPC20 event was set by the remote CPU
1: An IPC20 event was set by the remote CPU
Reset type: SYSRSn
19
IPC19
R
0h
Indicates to the local CPU if the IPC19 event flag was set by the 
remote CPU.
0: No IPC19 event was set by the remote CPU
1: An IPC19 event was set by the remote CPU
Reset type: SYSRSn
18
IPC18
R
0h
Indicates to the local CPU if the IPC18 event flag was set by the 
remote CPU.
0: No IPC18 event was set by the remote CPU
1: An IPC18 event was set by the remote CPU
Reset type: SYSRSn
17
IPC17
R
0h
Indicates to the local CPU if the IPC17 event flag was set by the 
remote CPU.
0: No IPC17 event was set by the remote CPU
1: An IPC17 event was set by the remote CPU
Reset type: SYSRSn
16
IPC16
R
0h
Indicates to the local CPU if the IPC16 event flag was set by the 
remote CPU.
0: No IPC16 event was set by the remote CPU
1: An IPC16 event was set by the remote CPU
Reset type: SYSRSn
15
IPC15
R
0h
Indicates to the local CPU if the IPC15 event flag was set by the 
remote CPU.
0: No IPC15 event was set by the remote CPU
1: An IPC15 event was set by the remote CPU
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
926
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
Table 7-7. IPCSTS Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
14
IPC14
R
0h
Indicates to the local CPU if the IPC14 event flag was set by the 
remote CPU.
0: No IPC14 event was set by the remote CPU
1: An IPC14 event was set by the remote CPU
Reset type: SYSRSn
13
IPC13
R
0h
Indicates to the local CPU if the IPC13 event flag was set by the 
remote CPU.
0: No IPC13 event was set by the remote CPU
1: An IPC13 event was set by the remote CPU
Reset type: SYSRSn
12
IPC12
R
0h
Indicates to the local CPU if the IPC12 event flag was set by the 
remote CPU.
0: No IPC12 event was set by the remote CPU
1: An IPC12 event was set by the remote CPU
Reset type: SYSRSn
11
IPC11
R
0h
Indicates to the local CPU if the IPC11 event flag was set by the 
remote CPU.
0: No IPC11 event was set by the remote CPU
1: An IPC11 event was set by the remote CPU
Reset type: SYSRSn
10
IPC10
R
0h
Indicates to the local CPU if the IPC10 event flag was set by the 
remote CPU.
0: No IPC10 event was set by the remote CPU
1: An IPC10 event was set by the remote CPU
Reset type: SYSRSn
9
IPC9
R
0h
Indicates to the local CPU if the IPC9 event flag was set by the 
remote CPU.
0: No IPC9 event was set by the remote CPU
1: An IPC9 event was set by the remote CPU
Reset type: SYSRSn
8
IPC8
R
0h
Indicates to the local CPU if the IPC8 event flag was set by the 
remote CPU.
0: No IPC8 event was set by the remote CPU
1: An IPC8 event was set by the remote CPU
Reset type: SYSRSn
7
IPC7
R
0h
Indicates to the local CPU if the IPC7 event flag was set by the 
remote CPU.
0: No IPC7 event was set by the remote CPU
1: An IPC7 event was set by the remote CPU
Reset type: SYSRSn
6
IPC6
R
0h
Indicates to the local CPU if the IPC6 event flag was set by the 
remote CPU.
0: No IPC6 event was set by the remote CPU
1: An IPC6 event was set by the remote CPU
Reset type: SYSRSn
5
IPC5
R
0h
Indicates to the local CPU if the IPC5 event flag was set by the 
remote CPU.
0: No IPC5 event was set by the remote CPU
1: An IPC5 event was set by the remote CPU
Reset type: SYSRSn
4
IPC4
R
0h
Indicates to the local CPU if the IPC4 event flag was set by the 
remote CPU.
0: No IPC4 event was set by the remote CPU
1: An IPC4 event was set by the remote CPU
Reset type: SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
927
Copyright © 2024 Texas Instruments Incorporated
Table 7-7. IPCSTS Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
3
IPC3
R
0h
Indicates to the local CPU if the IPC3 event flag was set by the 
remote CPU.
0: No IPC3 event was set by the remote CPU
1: An IPC3 event was set by the remote CPU
Notes
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
2
IPC2
R
0h
Indicates to the local CPU if the IPC2 event flag was set by the 
remote CPU.
0: No IPC2 event was set by the remote CPU
1: An IPC2 event was set by the remote CPU
Notes
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
1
IPC1
R
0h
Indicates to the local CPU if the IPC1 event flag was set by the 
remote CPU.
0: No IPC1 event was set by the remote CPU
1: An IPC1 event was set by the remote CPU
Notes
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
0
IPC0
R
0h
Indicates to the local CPU if the IPC0 event flag was set by the 
remote CPU.
0: No IPC0 event was set by the remote CPU
1: An IPC0 event was set by the remote CPU
Notes
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
928
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
7.7.2.3 IPCSET Register (Offset = 4h) [Reset = 00000000h] 
IPCSET is shown in Figure 7-4 and described in Table 7-8.
Return to the Summary Table.
IPC remote flag set register
Figure 7-4. IPCSET Register
31
30
29
28
27
26
25
24
IPC31
IPC30
IPC29
IPC28
IPC27
IPC26
IPC25
IPC24
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
23
22
21
20
19
18
17
16
IPC23
IPC22
IPC21
IPC20
IPC19
IPC18
IPC17
IPC16
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
15
14
13
12
11
10
9
8
IPC15
IPC14
IPC13
IPC12
IPC11
IPC10
IPC9
IPC8
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
7
6
5
4
3
2
1
0
IPC7
IPC6
IPC5
IPC4
IPC3
IPC2
IPC1
IPC0
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
Table 7-8. IPCSET Register Field Descriptions
Bit
Field
Type
Reset
Description
31
IPC31
R-0/W1S
0h
Writing 1 to this bit sets the IPC31 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
30
IPC30
R-0/W1S
0h
Writing 1 to this bit sets the IPC30 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
29
IPC29
R-0/W1S
0h
Writing 1 to this bit sets the IPC29 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
28
IPC28
R-0/W1S
0h
Writing 1 to this bit sets the IPC28 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
27
IPC27
R-0/W1S
0h
Writing 1 to this bit sets the IPC27 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
26
IPC26
R-0/W1S
0h
Writing 1 to this bit sets the IPC26 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
25
IPC25
R-0/W1S
0h
Writing 1 to this bit sets the IPC25 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
24
IPC24
R-0/W1S
0h
Writing 1 to this bit sets the IPC24 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
23
IPC23
R-0/W1S
0h
Writing 1 to this bit sets the IPC23 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
22
IPC22
R-0/W1S
0h
Writing 1 to this bit sets the IPC22 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
929
Copyright © 2024 Texas Instruments Incorporated
Table 7-8. IPCSET Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
21
IPC21
R-0/W1S
0h
Writing 1 to this bit sets the IPC21 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
20
IPC20
R-0/W1S
0h
Writing 1 to this bit sets the IPC20 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
19
IPC19
R-0/W1S
0h
Writing 1 to this bit sets the IPC19 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
18
IPC18
R-0/W1S
0h
Writing 1 to this bit sets the IPC18 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
17
IPC17
R-0/W1S
0h
Writing 1 to this bit sets the IPC17 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
16
IPC16
R-0/W1S
0h
Writing 1 to this bit sets the IPC16 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
15
IPC15
R-0/W1S
0h
Writing 1 to this bit sets the IPC15 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
14
IPC14
R-0/W1S
0h
Writing 1 to this bit sets the IPC14 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
13
IPC13
R-0/W1S
0h
Writing 1 to this bit sets the IPC13 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
12
IPC12
R-0/W1S
0h
Writing 1 to this bit sets the IPC12 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
11
IPC11
R-0/W1S
0h
Writing 1 to this bit sets the IPC11 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
10
IPC10
R-0/W1S
0h
Writing 1 to this bit sets the IPC10 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
9
IPC9
R-0/W1S
0h
Writing 1 to this bit sets the IPC9 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
8
IPC8
R-0/W1S
0h
Writing 1 to this bit sets the IPC8 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
7
IPC7
R-0/W1S
0h
Writing 1 to this bit sets the IPC7 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
6
IPC6
R-0/W1S
0h
Writing 1 to this bit sets the IPC6 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
5
IPC5
R-0/W1S
0h
Writing 1 to this bit sets the IPC5 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
930
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
Table 7-8. IPCSET Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
4
IPC4
R-0/W1S
0h
Writing 1 to this bit sets the IPC4 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
3
IPC3
R-0/W1S
0h
Writing 1 to this bit sets the IPC3 event flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
2
IPC2
R-0/W1S
0h
Writing 1 to this bit sets the IPC2 event flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
1
IPC1
R-0/W1S
0h
Writing 1 to this bit sets the IPC1 event flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
0
IPC0
R-0/W1S
0h
Writing 1 to this bit sets the IPC0 event flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
931
Copyright © 2024 Texas Instruments Incorporated
7.7.2.4 IPCCLR Register (Offset = 6h) [Reset = 00000000h] 
IPCCLR is shown in Figure 7-5 and described in Table 7-9.
Return to the Summary Table.
IPC remote flag clear register
Figure 7-5. IPCCLR Register
31
30
29
28
27
26
25
24
IPC31
IPC30
IPC29
IPC28
IPC27
IPC26
IPC25
IPC24
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
23
22
21
20
19
18
17
16
IPC23
IPC22
IPC21
IPC20
IPC19
IPC18
IPC17
IPC16
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
15
14
13
12
11
10
9
8
IPC15
IPC14
IPC13
IPC12
IPC11
IPC10
IPC9
IPC8
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
7
6
5
4
3
2
1
0
IPC7
IPC6
IPC5
IPC4
IPC3
IPC2
IPC1
IPC0
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
Table 7-9. IPCCLR Register Field Descriptions
Bit
Field
Type
Reset
Description
31
IPC31
R-0/W1S
0h
Writing 1 to this bit clears the IPC31 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
30
IPC30
R-0/W1S
0h
Writing 1 to this bit clears the IPC30 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
29
IPC29
R-0/W1S
0h
Writing 1 to this bit clears the IPC29 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
28
IPC28
R-0/W1S
0h
Writing 1 to this bit clears the IPC28 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
932
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
Table 7-9. IPCCLR Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
27
IPC27
R-0/W1S
0h
Writing 1 to this bit clears the IPC27 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
26
IPC26
R-0/W1S
0h
Writing 1 to this bit clears the IPC26 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
25
IPC25
R-0/W1S
0h
Writing 1 to this bit clears the IPC25 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
24
IPC24
R-0/W1S
0h
Writing 1 to this bit clears the IPC24 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
23
IPC23
R-0/W1S
0h
Writing 1 to this bit clears the IPC23 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
22
IPC22
R-0/W1S
0h
Writing 1 to this bit clears the IPC22 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
21
IPC21
R-0/W1S
0h
Writing 1 to this bit clears the IPC21 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
20
IPC20
R-0/W1S
0h
Writing 1 to this bit clears the IPC20 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
933
Copyright © 2024 Texas Instruments Incorporated
Table 7-9. IPCCLR Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
19
IPC19
R-0/W1S
0h
Writing 1 to this bit clears the IPC19 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
18
IPC18
R-0/W1S
0h
Writing 1 to this bit clears the IPC18 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
17
IPC17
R-0/W1S
0h
Writing 1 to this bit clears the IPC17 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
16
IPC16
R-0/W1S
0h
Writing 1 to this bit clears the IPC16 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
15
IPC15
R-0/W1S
0h
Writing 1 to this bit clears the IPC15 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
14
IPC14
R-0/W1S
0h
Writing 1 to this bit clears the IPC14 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
13
IPC13
R-0/W1S
0h
Writing 1 to this bit clears the IPC13 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
12
IPC12
R-0/W1S
0h
Writing 1 to this bit clears the IPC12 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
934
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
Table 7-9. IPCCLR Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
11
IPC11
R-0/W1S
0h
Writing 1 to this bit clears the IPC11 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
10
IPC10
R-0/W1S
0h
Writing 1 to this bit clears the IPC10 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
9
IPC9
R-0/W1S
0h
Writing 1 to this bit clears the IPC9 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
8
IPC8
R-0/W1S
0h
Writing 1 to this bit clears the IPC8 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
7
IPC7
R-0/W1S
0h
Writing 1 to this bit clears the IPC7 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
6
IPC6
R-0/W1S
0h
Writing 1 to this bit clears the IPC6 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
5
IPC5
R-0/W1S
0h
Writing 1 to this bit clears the IPC5 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
4
IPC4
R-0/W1S
0h
Writing 1 to this bit clears the IPC4 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
935
Copyright © 2024 Texas Instruments Incorporated
Table 7-9. IPCCLR Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
3
IPC3
R-0/W1S
0h
Writing 1 to this bit clears the IPC3 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
2
IPC2
R-0/W1S
0h
Writing 1 to this bit clears the IPC2 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
1
IPC1
R-0/W1S
0h
Writing 1 to this bit clears the IPC1 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
0
IPC0
R-0/W1S
0h
Writing 1 to this bit clears the IPC0 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
936
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
7.7.2.5 IPCFLG Register (Offset = 8h) [Reset = 00000000h] 
IPCFLG is shown in Figure 7-6 and described in Table 7-10.
Return to the Summary Table.
IPC remote flag status register
Figure 7-6. IPCFLG Register
31
30
29
28
27
26
25
24
IPC31
IPC30
IPC29
IPC28
IPC27
IPC26
IPC25
IPC24
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
23
22
21
20
19
18
17
16
IPC23
IPC22
IPC21
IPC20
IPC19
IPC18
IPC17
IPC16
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
15
14
13
12
11
10
9
8
IPC15
IPC14
IPC13
IPC12
IPC11
IPC10
IPC9
IPC8
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
7
6
5
4
3
2
1
0
IPC7
IPC6
IPC5
IPC4
IPC3
IPC2
IPC1
IPC0
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
Table 7-10. IPCFLG Register Field Descriptions
Bit
Field
Type
Reset
Description
31
IPC31
R
0h
Indicates to the local CPU whether the remote IPC31 event flag is 
set.
0: The remote IPC31 event flag is not set
1: The remote IPC31 event flag is set
Reset type: SYSRSn
30
IPC30
R
0h
Indicates to the local CPU whether the remote IPC30 event flag is 
set.
0: The remote IPC30 event flag is not set
1: The remote IPC30 event flag is set
Reset type: SYSRSn
29
IPC29
R
0h
Indicates to the local CPU whether the remote IPC29 event flag is 
set.
0: The remote IPC29 event flag is not set
1: The remote IPC29 event flag is set
Reset type: SYSRSn
28
IPC28
R
0h
Indicates to the local CPU whether the remote IPC28 event flag is 
set.
0: The remote IPC28 event flag is not set
1: The remote IPC28 event flag is set
Reset type: SYSRSn
27
IPC27
R
0h
Indicates to the local CPU whether the remote IPC27 event flag is 
set.
0: The remote IPC27 event flag is not set
1: The remote IPC27 event flag is set
Reset type: SYSRSn
26
IPC26
R
0h
Indicates to the local CPU whether the remote IPC26 event flag is 
set.
0: The remote IPC26 event flag is not set
1: The remote IPC26 event flag is set
Reset type: SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
937
Copyright © 2024 Texas Instruments Incorporated
Table 7-10. IPCFLG Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
25
IPC25
R
0h
Indicates to the local CPU whether the remote IPC25 event flag is 
set.
0: The remote IPC25 event flag is not set
1: The remote IPC25 event flag is set
Reset type: SYSRSn
24
IPC24
R
0h
Indicates to the local CPU whether the remote IPC24 event flag is 
set.
0: The remote IPC24 event flag is not set
1: The remote IPC24 event flag is set
Reset type: SYSRSn
23
IPC23
R
0h
Indicates to the local CPU whether the remote IPC23 event flag is 
set.
0: The remote IPC23 event flag is not set
1: The remote IPC23 event flag is set
Reset type: SYSRSn
22
IPC22
R
0h
Indicates to the local CPU whether the remote IPC22 event flag is 
set.
0: The remote IPC22 event flag is not set
1: The remote IPC22 event flag is set
Reset type: SYSRSn
21
IPC21
R
0h
Indicates to the local CPU whether the remote IPC21 event flag is 
set.
0: The remote IPC21 event flag is not set
1: The remote IPC21 event flag is set
Reset type: SYSRSn
20
IPC20
R
0h
Indicates to the local CPU whether the remote IPC20 event flag is 
set.
0: The remote IPC20 event flag is not set
1: The remote IPC20 event flag is set
Reset type: SYSRSn
19
IPC19
R
0h
Indicates to the local CPU whether the remote IPC19 event flag is 
set.
0: The remote IPC19 event flag is not set
1: The remote IPC19 event flag is set
Reset type: SYSRSn
18
IPC18
R
0h
Indicates to the local CPU whether the remote IPC18 event flag is 
set.
0: The remote IPC18 event flag is not set
1: The remote IPC18 event flag is set
Reset type: SYSRSn
17
IPC17
R
0h
Indicates to the local CPU whether the remote IPC17 event flag is 
set.
0: The remote IPC17 event flag is not set
1: The remote IPC17 event flag is set
Reset type: SYSRSn
16
IPC16
R
0h
Indicates to the local CPU whether the remote IPC16 event flag is 
set.
0: The remote IPC16 event flag is not set
1: The remote IPC16 event flag is set
Reset type: SYSRSn
15
IPC15
R
0h
Indicates to the local CPU whether the remote IPC15 event flag is 
set.
0: The remote IPC15 event flag is not set
1: The remote IPC15 event flag is set
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
938
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
Table 7-10. IPCFLG Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
14
IPC14
R
0h
Indicates to the local CPU whether the remote IPC14 event flag is 
set.
0: The remote IPC14 event flag is not set
1: The remote IPC14 event flag is set
Reset type: SYSRSn
13
IPC13
R
0h
Indicates to the local CPU whether the remote IPC13 event flag is 
set.
0: The remote IPC13 event flag is not set
1: The remote IPC13 event flag is set
Reset type: SYSRSn
12
IPC12
R
0h
Indicates to the local CPU whether the remote IPC12 event flag is 
set.
0: The remote IPC12 event flag is not set
1: The remote IPC12 event flag is set
Reset type: SYSRSn
11
IPC11
R
0h
Indicates to the local CPU whether the remote IPC11 event flag is 
set.
0: The remote IPC11 event flag is not set
1: The remote IPC11 event flag is set
Reset type: SYSRSn
10
IPC10
R
0h
Indicates to the local CPU whether the remote IPC10 event flag is 
set.
0: The remote IPC10 event flag is not set
1: The remote IPC10 event flag is set
Reset type: SYSRSn
9
IPC9
R
0h
Indicates to the local CPU whether the remote IPC9 event flag is set.
0: The remote IPC9 event flag is not set
1: The remote IPC9 event flag is set
Reset type: SYSRSn
8
IPC8
R
0h
Indicates to the local CPU whether the remote IPC8 event flag is set.
0: The remote IPC8 event flag is not set
1: The remote IPC8 event flag is set
Reset type: SYSRSn
7
IPC7
R
0h
Indicates to the local CPU whether the remote IPC7 event flag is set.
0: The remote IPC7 event flag is not set
1: The remote IPC7 event flag is set
Reset type: SYSRSn
6
IPC6
R
0h
Indicates to the local CPU whether the remote IPC6 event flag is set.
0: The remote IPC6 event flag is not set
1: The remote IPC6 event flag is set
Reset type: SYSRSn
5
IPC5
R
0h
Indicates to the local CPU whether the remote IPC5 event flag is set.
0: The remote IPC5 event flag is not set
1: The remote IPC5 event flag is set
Reset type: SYSRSn
4
IPC4
R
0h
Indicates to the local CPU whether the remote IPC4 event flag is set.
0: The remote IPC4 event flag is not set
1: The remote IPC4 event flag is set
Reset type: SYSRSn
3
IPC3
R
0h
Indicates to the local CPU whether the remote IPC3 event flag is set.
0: The remote IPC3 event flag is not set
1: The remote IPC3 event flag is set
Notes:
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
939
Copyright © 2024 Texas Instruments Incorporated
Table 7-10. IPCFLG Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
2
IPC2
R
0h
Indicates to the local CPU whether the remote IPC2 event flag is set.
0: The remote IPC2 event flag is not set
1: The remote IPC2 event flag is set
Notes:
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
1
IPC1
R
0h
Indicates to the local CPU whether the remote IPC1 event flag is set.
0: The remote IPC1 event flag is not set
1: The remote IPC1 event flag is set
Notes:
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
0
IPC0
R
0h
Indicates to the local CPU whether the remote IPC0 event flag is set.
0: The remote IPC0 event flag is not set
1: The remote IPC0 event flag is set
Notes:
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
940
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
7.7.2.6 IPCCOUNTERL Register (Offset = Ch) [Reset = 00000000h] 
IPCCOUNTERL is shown in Figure 7-7 and described in Table 7-11.
Return to the Summary Table.
IPC Counter Low Register
Figure 7-7. IPCCOUNTERL Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
COUNT
R-0h
Table 7-11. IPCCOUNTERL Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
COUNT
R
0h
This is the lower 32-bits of free running 64 bit timestamp counter 
clocked by the PLLSYSCLK.
Reset type: CPU1.SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
941
Copyright © 2024 Texas Instruments Incorporated
7.7.2.7 IPCCOUNTERH Register (Offset = Eh) [Reset = 00000000h] 
IPCCOUNTERH is shown in Figure 7-8 and described in Table 7-12.
Return to the Summary Table.
IPC Counter High Register
Figure 7-8. IPCCOUNTERH Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
COUNT
R-0h
Table 7-12. IPCCOUNTERH Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
COUNT
R
0h
This is the upper 32-bits of free running 64 bit timestamp counter 
clocked by the PLLSYSCLK.
Reset type: CPU1.SYSRSn
Interprocessor Communication (IPC)
www.ti.com
942
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
7.7.2.8 IPCSENDCOM Register (Offset = 10h) [Reset = 00000000h] 
IPCSENDCOM is shown in Figure 7-9 and described in Table 7-13.
Return to the Summary Table.
Local to Remote IPC Command Register
Figure 7-9. IPCSENDCOM Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
COMMAND
R/W-0h
Table 7-13. IPCSENDCOM Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
COMMAND
R/W
0h
This is a general purpose register used to send software-defined 
commands to the remote CPU. It can only be written by the local 
CPU.
Notes
[1] The local CPU's IPCSENDCOM is the same physical register 
as the remote CPU's IPCRECVCOM, and is located at the same 
address in both CPUs.
Reset type: SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
943
Copyright © 2024 Texas Instruments Incorporated
7.7.2.9 IPCSENDADDR Register (Offset = 12h) [Reset = 00000000h] 
IPCSENDADDR is shown in Figure 7-10 and described in Table 7-14.
Return to the Summary Table.
Local to Remote IPC Address Register
Figure 7-10. IPCSENDADDR Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
ADDRESS
R/W-0h
Table 7-14. IPCSENDADDR Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
ADDRESS
R/W
0h
This is a general purpose register used to send software-defined 
addresses to the remote CPU. It can only be written by the local 
CPU.
Notes
[1] The local CPU's IPCSENDADDR is the same physical register 
as the remote CPU's IPCRECVDATA, and is located at the same 
address in both CPUs.
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
944
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
7.7.2.10 IPCSENDDATA Register (Offset = 14h) [Reset = 00000000h] 
IPCSENDDATA is shown in Figure 7-11 and described in Table 7-15.
Return to the Summary Table.
Local to Remote IPC Data Register
Figure 7-11. IPCSENDDATA Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
WDATA
R/W-0h
Table 7-15. IPCSENDDATA Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
WDATA
R/W
0h
This is a general purpose register used to send software-defined 
data to the remote CPU. It can only be written by the local CPU.
Notes
[1] The local CPU's IPCSENDDATA is the same physical register 
as the remote CPU's IPCRECVDATA, and is located at the same 
address in both CPUs.
Reset type: SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
945
Copyright © 2024 Texas Instruments Incorporated
7.7.2.11 IPCREMOTEREPLY Register (Offset = 16h) [Reset = 00000000h] 
IPCREMOTEREPLY is shown in Figure 7-12 and described in Table 7-16.
Return to the Summary Table.
Remote to Local IPC Reply Data Register
Figure 7-12. IPCREMOTEREPLY Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
RDATA
R-0h
Table 7-16. IPCREMOTEREPLY Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
RDATA
R
0h
This is a general purpose register used to receive software-defined 
data from the remote CPU's response to a command. It can only be 
written by the remote CPU.
Notes
[1] The local CPU's IPCREMOTEREPLY is the same physical 
register as the remote CPU's IPCLOCALREPLY, and is located at 
the same address in both CPUs.
[2] This register is reset by a SYRSn of the remote CPU
Reset type: CPUx.SYSRSn
Interprocessor Communication (IPC)
www.ti.com
946
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
7.7.2.12 IPCRECVCOM Register (Offset = 18h) [Reset = 00000000h] 
IPCRECVCOM is shown in Figure 7-13 and described in Table 7-17.
Return to the Summary Table.
Remote to Local IPC Command Register
Figure 7-13. IPCRECVCOM Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
COMMAND
R-0h
Table 7-17. IPCRECVCOM Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
COMMAND
R
0h
This is a general purpose register used to receive software-defined 
commands from the remote CPU. It can only be written by the 
remote CPU.
Notes
[1] The local CPU's IPCRECVCOM is the same physical register 
as the remote CPU's IPCSENDCOM, and is located at the same 
address in both CPUs.
[2] This register is reset by a SYRSn of the remote CPU
Reset type: CPUx.SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
947
Copyright © 2024 Texas Instruments Incorporated
7.7.2.13 IPCRECVADDR Register (Offset = 1Ah) [Reset = 00000000h] 
IPCRECVADDR is shown in Figure 7-14 and described in Table 7-18.
Return to the Summary Table.
Remote to Local IPC Address Register
Figure 7-14. IPCRECVADDR Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
ADDRESS
R-0h
Table 7-18. IPCRECVADDR Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
ADDRESS
R
0h
This is a general purpose register used to receive software-defined 
addresses from the remote CPU. It can only be written by the remote 
CPU.
Notes
[1] The local CPU's IPCRECVADDR is the same physical register 
as the remote CPU's IPCSENDADDR, and is located at the same 
address in both CPUs.
[2] This register is reset by a SYRSn of the remote CPU
Reset type: CPUx.SYSRSn
Interprocessor Communication (IPC)
www.ti.com
948
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
7.7.2.14 IPCRECVDATA Register (Offset = 1Ch) [Reset = 00000000h] 
IPCRECVDATA is shown in Figure 7-15 and described in Table 7-19.
Return to the Summary Table.
Remote to Local IPC Data Register
Figure 7-15. IPCRECVDATA Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
WDATA
R-0h
Table 7-19. IPCRECVDATA Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
WDATA
R
0h
This is a general purpose register used to receive software-defined 
data from the remote CPU. It can only be written by the remote CPU.
Notes
[1] The local CPU's IPCRECVDATA is the same physical register 
as the remote CPU's IPCSENDDATA, and is located at the same 
address in both CPUs.
[2] This register is reset by a SYRSn of the remote CPU
Reset type: CPUx.SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
949
Copyright © 2024 Texas Instruments Incorporated
7.7.2.15 IPCLOCALREPLY Register (Offset = 1Eh) [Reset = 00000000h] 
IPCLOCALREPLY is shown in Figure 7-16 and described in Table 7-20.
Return to the Summary Table.
Local to Remote IPC Reply Data Register
Figure 7-16. IPCLOCALREPLY Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
RDATA
R/W-0h
Table 7-20. IPCLOCALREPLY Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
RDATA
R/W
0h
This is a general purpose register used to send software-defined 
data to the remote CPU in response to a command. It can only be 
written by the local CPU.
Notes
[1] The local CPU's IPCLOCALREPLY is the same physical register 
as the remote CPU's IPCREMOTEREPLY, and is located at the 
same address in both CPUs.
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
950
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
7.7.2.16 IPCBOOTSTS Register (Offset = 20h) [Reset = 00000000h] 
IPCBOOTSTS is shown in Figure 7-17 and described in Table 7-21.
Return to the Summary Table.
CPU2 to CPU1 IPC Boot Status Register
Figure 7-17. IPCBOOTSTS Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
BOOTSTS
R/W-0h
Table 7-21. IPCBOOTSTS Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
BOOTSTS
R/W
0h
This register is used by CPU2 to pass the boot Status to CPU1. The 
data format is software-defined. It can only be written by CPU2.
Reset type: CPU2.SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
951
Copyright © 2024 Texas Instruments Incorporated
7.7.2.17 IPCBOOTMODE Register (Offset = 22h) [Reset = 00000000h] 
IPCBOOTMODE is shown in Figure 7-18 and described in Table 7-22.
Return to the Summary Table.
CPU1 to CPU2 IPC Boot Mode Register
Figure 7-18. IPCBOOTMODE Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
BOOTMODE
R/W-0h
Table 7-22. IPCBOOTMODE Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
BOOTMODE
R/W
0h
This register is used by CPU1 to pass a boot mode information to 
CPU2. The data format is software-defined. It can only be written by 
CPU1.
Reset type: CPU1.SYSRSn
Interprocessor Communication (IPC)
www.ti.com
952
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
7.7.3 IPC_REGS_CPU2 Registers
Table 7-23 lists the memory-mapped registers for the IPC_REGS_CPU2 registers. All register offset addresses 
not listed in Table 7-23 should be considered as reserved locations and the register contents should not be 
modified.
Table 7-23. IPC_REGS_CPU2 Registers
Offset
Acronym
Register Name
Write Protection
Section
0h
IPCACK
IPC incoming flag clear (acknowledge) register
Go
2h
IPCSTS
IPC incoming flag status register
Go
4h
IPCSET
IPC remote flag set register
Go
6h
IPCCLR
IPC remote flag clear register
Go
8h
IPCFLG
IPC remote flag status register
Go
Ch
IPCCOUNTERL
IPC Counter Low Register
Go
Eh
IPCCOUNTERH
IPC Counter High Register
Go
10h
IPCRECVCOM
Remote to Local IPC Command Register
Go
12h
IPCRECVADDR
Remote to Local IPC Address Register
Go
14h
IPCRECVDATA
Remote to Local IPC Data Register
Go
16h
IPCLOCALREPLY
Local to Remote IPC Reply Data Register
Go
18h
IPCSENDCOM
Local to Remote IPC Command Register
Go
1Ah
IPCSENDADDR
Local to Remote IPC Address Register
Go
1Ch
IPCSENDDATA
Local to Remote IPC Data Register
Go
1Eh
IPCREMOTEREPLY
Remote to Local IPC Reply Data Register
Go
20h
IPCBOOTSTS
CPU2 to CPU1 IPC Boot Status Register
Go
22h
IPCBOOTMODE
CPU1 to CPU2 IPC Boot Mode Register
Go
Complex bit access types are encoded to fit into small table cells. Table 7-24 shows the codes that are used for 
access types in this section.
Table 7-24. IPC_REGS_CPU2 Access Type Codes
Access Type
Code
Description
Read Type
R
R
Read
R-0
R
-0
Read
Returns 0s
Write Type
W
W
Write
W1S
W
1S
Write
1 to set
Reset or Default Value
-n
Value after reset or the default value
Register Array Variables
i,j,k,l,m,n
When these variables are used in a register name, 
an offset, or an address, they refer to the value of a 
register array where the register is part of a group 
of repeating registers. The register groups form a 
hierarchical structure and the array is represented 
with a formula.
y
When this variable is used in a register name, an 
offset, or an address it refers to the value of a 
register array.
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
953
Copyright © 2024 Texas Instruments Incorporated
7.7.3.1 IPCACK Register (Offset = 0h) [Reset = 00000000h] 
IPCACK is shown in Figure 7-19 and described in Table 7-25.
Return to the Summary Table.
IPC incoming flag clear (acknowledge) register
Figure 7-19. IPCACK Register
31
30
29
28
27
26
25
24
IPC31
IPC30
IPC29
IPC28
IPC27
IPC26
IPC25
IPC24
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
23
22
21
20
19
18
17
16
IPC23
IPC22
IPC21
IPC20
IPC19
IPC18
IPC17
IPC16
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
15
14
13
12
11
10
9
8
IPC15
IPC14
IPC13
IPC12
IPC11
IPC10
IPC9
IPC8
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
7
6
5
4
3
2
1
0
IPC7
IPC6
IPC5
IPC4
IPC3
IPC2
IPC1
IPC0
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
Table 7-25. IPCACK Register Field Descriptions
Bit
Field
Type
Reset
Description
31
IPC31
R-0/W1S
0h
Writing 1 to this bit clears the IPC31 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
30
IPC30
R-0/W1S
0h
Writing 1 to this bit clears the IPC30 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
29
IPC29
R-0/W1S
0h
Writing 1 to this bit clears the IPC29 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
28
IPC28
R-0/W1S
0h
Writing 1 to this bit clears the IPC28 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
27
IPC27
R-0/W1S
0h
Writing 1 to this bit clears the IPC27 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
26
IPC26
R-0/W1S
0h
Writing 1 to this bit clears the IPC26 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
25
IPC25
R-0/W1S
0h
Writing 1 to this bit clears the IPC25 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
954
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
Table 7-25. IPCACK Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
24
IPC24
R-0/W1S
0h
Writing 1 to this bit clears the IPC24 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
23
IPC23
R-0/W1S
0h
Writing 1 to this bit clears the IPC23 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
22
IPC22
R-0/W1S
0h
Writing 1 to this bit clears the IPC22 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
21
IPC21
R-0/W1S
0h
Writing 1 to this bit clears the IPC21 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
20
IPC20
R-0/W1S
0h
Writing 1 to this bit clears the IPC20 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
19
IPC19
R-0/W1S
0h
Writing 1 to this bit clears the IPC19 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
18
IPC18
R-0/W1S
0h
Writing 1 to this bit clears the IPC18 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
17
IPC17
R-0/W1S
0h
Writing 1 to this bit clears the IPC17 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
16
IPC16
R-0/W1S
0h
Writing 1 to this bit clears the IPC16 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
15
IPC15
R-0/W1S
0h
Writing 1 to this bit clears the IPC15 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
14
IPC14
R-0/W1S
0h
Writing 1 to this bit clears the IPC14 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
13
IPC13
R-0/W1S
0h
Writing 1 to this bit clears the IPC13 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
12
IPC12
R-0/W1S
0h
Writing 1 to this bit clears the IPC12 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
11
IPC11
R-0/W1S
0h
Writing 1 to this bit clears the IPC11 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
955
Copyright © 2024 Texas Instruments Incorporated
Table 7-25. IPCACK Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
10
IPC10
R-0/W1S
0h
Writing 1 to this bit clears the IPC10 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
9
IPC9
R-0/W1S
0h
Writing 1 to this bit clears the IPC9 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
8
IPC8
R-0/W1S
0h
Writing 1 to this bit clears the IPC8 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
7
IPC7
R-0/W1S
0h
Writing 1 to this bit clears the IPC7 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
6
IPC6
R-0/W1S
0h
Writing 1 to this bit clears the IPC6 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
5
IPC5
R-0/W1S
0h
Writing 1 to this bit clears the IPC5 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
4
IPC4
R-0/W1S
0h
Writing 1 to this bit clears the IPC4 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
3
IPC3
R-0/W1S
0h
Writing 1 to this bit clears the IPC3 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
2
IPC2
R-0/W1S
0h
Writing 1 to this bit clears the IPC2 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
1
IPC1
R-0/W1S
0h
Writing 1 to this bit clears the IPC1 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
0
IPC0
R-0/W1S
0h
Writing 1 to this bit clears the IPC0 event flag which was set by the 
remote CPU.
Writing 0 to this bit has no effect.
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
956
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
7.7.3.2 IPCSTS Register (Offset = 2h) [Reset = 00000000h] 
IPCSTS is shown in Figure 7-20 and described in Table 7-26.
Return to the Summary Table.
IPC incoming flag status register
Figure 7-20. IPCSTS Register
31
30
29
28
27
26
25
24
IPC31
IPC30
IPC29
IPC28
IPC27
IPC26
IPC25
IPC24
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
23
22
21
20
19
18
17
16
IPC23
IPC22
IPC21
IPC20
IPC19
IPC18
IPC17
IPC16
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
15
14
13
12
11
10
9
8
IPC15
IPC14
IPC13
IPC12
IPC11
IPC10
IPC9
IPC8
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
7
6
5
4
3
2
1
0
IPC7
IPC6
IPC5
IPC4
IPC3
IPC2
IPC1
IPC0
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
Table 7-26. IPCSTS Register Field Descriptions
Bit
Field
Type
Reset
Description
31
IPC31
R
0h
Indicates to the local CPU if the IPC31 event flag was set by the 
remote CPU.
0: No IPC31 event was set by the remote CPU
1: An IPC31 event was set by the remote CPU
Reset type: SYSRSn
30
IPC30
R
0h
Indicates to the local CPU if the IPC30 event flag was set by the 
remote CPU.
0: No IPC30 event was set by the remote CPU
1: An IPC30 event was set by the remote CPU
Reset type: SYSRSn
29
IPC29
R
0h
Indicates to the local CPU if the IPC29 event flag was set by the 
remote CPU.
0: No IPC29 event was set by the remote CPU
1: An IPC29 event was set by the remote CPU
Reset type: SYSRSn
28
IPC28
R
0h
Indicates to the local CPU if the IPC28 event flag was set by the 
remote CPU.
0: No IPC28 event was set by the remote CPU
1: An IPC28 event was set by the remote CPU
Reset type: SYSRSn
27
IPC27
R
0h
Indicates to the local CPU if the IPC27 event flag was set by the 
remote CPU.
0: No IPC27 event was set by the remote CPU
1: An IPC27 event was set by the remote CPU
Reset type: SYSRSn
26
IPC26
R
0h
Indicates to the local CPU if the IPC26 event flag was set by the 
remote CPU.
0: No IPC26 event was set by the remote CPU
1: An IPC26 event was set by the remote CPU
Reset type: SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
957
Copyright © 2024 Texas Instruments Incorporated
Table 7-26. IPCSTS Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
25
IPC25
R
0h
Indicates to the local CPU if the IPC25 event flag was set by the 
remote CPU.
0: No IPC25 event was set by the remote CPU
1: An IPC25 event was set by the remote CPU
Reset type: SYSRSn
24
IPC24
R
0h
Indicates to the local CPU if the IPC24 event flag was set by the 
remote CPU.
0: No IPC24 event was set by the remote CPU
1: An IPC24 event was set by the remote CPU
Reset type: SYSRSn
23
IPC23
R
0h
Indicates to the local CPU if the IPC23 event flag was set by the 
remote CPU.
0: No IPC23 event was set by the remote CPU
1: An IPC23 event was set by the remote CPU
Reset type: SYSRSn
22
IPC22
R
0h
Indicates to the local CPU if the IPC22 event flag was set by the 
remote CPU.
0: No IPC22 event was set by the remote CPU
1: An IPC22 event was set by the remote CPU
Reset type: SYSRSn
21
IPC21
R
0h
Indicates to the local CPU if the IPC21 event flag was set by the 
remote CPU.
0: No IPC21 event was set by the remote CPU
1: An IPC21 event was set by the remote CPU
Reset type: SYSRSn
20
IPC20
R
0h
Indicates to the local CPU if the IPC20 event flag was set by the 
remote CPU.
0: No IPC20 event was set by the remote CPU
1: An IPC20 event was set by the remote CPU
Reset type: SYSRSn
19
IPC19
R
0h
Indicates to the local CPU if the IPC19 event flag was set by the 
remote CPU.
0: No IPC19 event was set by the remote CPU
1: An IPC19 event was set by the remote CPU
Reset type: SYSRSn
18
IPC18
R
0h
Indicates to the local CPU if the IPC18 event flag was set by the 
remote CPU.
0: No IPC18 event was set by the remote CPU
1: An IPC18 event was set by the remote CPU
Reset type: SYSRSn
17
IPC17
R
0h
Indicates to the local CPU if the IPC17 event flag was set by the 
remote CPU.
0: No IPC17 event was set by the remote CPU
1: An IPC17 event was set by the remote CPU
Reset type: SYSRSn
16
IPC16
R
0h
Indicates to the local CPU if the IPC16 event flag was set by the 
remote CPU.
0: No IPC16 event was set by the remote CPU
1: An IPC16 event was set by the remote CPU
Reset type: SYSRSn
15
IPC15
R
0h
Indicates to the local CPU if the IPC15 event flag was set by the 
remote CPU.
0: No IPC15 event was set by the remote CPU
1: An IPC15 event was set by the remote CPU
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
958
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
Table 7-26. IPCSTS Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
14
IPC14
R
0h
Indicates to the local CPU if the IPC14 event flag was set by the 
remote CPU.
0: No IPC14 event was set by the remote CPU
1: An IPC14 event was set by the remote CPU
Reset type: SYSRSn
13
IPC13
R
0h
Indicates to the local CPU if the IPC13 event flag was set by the 
remote CPU.
0: No IPC13 event was set by the remote CPU
1: An IPC13 event was set by the remote CPU
Reset type: SYSRSn
12
IPC12
R
0h
Indicates to the local CPU if the IPC12 event flag was set by the 
remote CPU.
0: No IPC12 event was set by the remote CPU
1: An IPC12 event was set by the remote CPU
Reset type: SYSRSn
11
IPC11
R
0h
Indicates to the local CPU if the IPC11 event flag was set by the 
remote CPU.
0: No IPC11 event was set by the remote CPU
1: An IPC11 event was set by the remote CPU
Reset type: SYSRSn
10
IPC10
R
0h
Indicates to the local CPU if the IPC10 event flag was set by the 
remote CPU.
0: No IPC10 event was set by the remote CPU
1: An IPC10 event was set by the remote CPU
Reset type: SYSRSn
9
IPC9
R
0h
Indicates to the local CPU if the IPC9 event flag was set by the 
remote CPU.
0: No IPC9 event was set by the remote CPU
1: An IPC9 event was set by the remote CPU
Reset type: SYSRSn
8
IPC8
R
0h
Indicates to the local CPU if the IPC8 event flag was set by the 
remote CPU.
0: No IPC8 event was set by the remote CPU
1: An IPC8 event was set by the remote CPU
Reset type: SYSRSn
7
IPC7
R
0h
Indicates to the local CPU if the IPC7 event flag was set by the 
remote CPU.
0: No IPC7 event was set by the remote CPU
1: An IPC7 event was set by the remote CPU
Reset type: SYSRSn
6
IPC6
R
0h
Indicates to the local CPU if the IPC6 event flag was set by the 
remote CPU.
0: No IPC6 event was set by the remote CPU
1: An IPC6 event was set by the remote CPU
Reset type: SYSRSn
5
IPC5
R
0h
Indicates to the local CPU if the IPC5 event flag was set by the 
remote CPU.
0: No IPC5 event was set by the remote CPU
1: An IPC5 event was set by the remote CPU
Reset type: SYSRSn
4
IPC4
R
0h
Indicates to the local CPU if the IPC4 event flag was set by the 
remote CPU.
0: No IPC4 event was set by the remote CPU
1: An IPC4 event was set by the remote CPU
Reset type: SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
959
Copyright © 2024 Texas Instruments Incorporated
Table 7-26. IPCSTS Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
3
IPC3
R
0h
Indicates to the local CPU if the IPC3 event flag was set by the 
remote CPU.
0: No IPC3 event was set by the remote CPU
1: An IPC3 event was set by the remote CPU
Notes
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
2
IPC2
R
0h
Indicates to the local CPU if the IPC2 event flag was set by the 
remote CPU.
0: No IPC2 event was set by the remote CPU
1: An IPC2 event was set by the remote CPU
Notes
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
1
IPC1
R
0h
Indicates to the local CPU if the IPC1 event flag was set by the 
remote CPU.
0: No IPC1 event was set by the remote CPU
1: An IPC1 event was set by the remote CPU
Notes
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
0
IPC0
R
0h
Indicates to the local CPU if the IPC0 event flag was set by the 
remote CPU.
0: No IPC0 event was set by the remote CPU
1: An IPC0 event was set by the remote CPU
Notes
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
960
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
7.7.3.3 IPCSET Register (Offset = 4h) [Reset = 00000000h] 
IPCSET is shown in Figure 7-21 and described in Table 7-27.
Return to the Summary Table.
IPC remote flag set register
Figure 7-21. IPCSET Register
31
30
29
28
27
26
25
24
IPC31
IPC30
IPC29
IPC28
IPC27
IPC26
IPC25
IPC24
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
23
22
21
20
19
18
17
16
IPC23
IPC22
IPC21
IPC20
IPC19
IPC18
IPC17
IPC16
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
15
14
13
12
11
10
9
8
IPC15
IPC14
IPC13
IPC12
IPC11
IPC10
IPC9
IPC8
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
7
6
5
4
3
2
1
0
IPC7
IPC6
IPC5
IPC4
IPC3
IPC2
IPC1
IPC0
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
Table 7-27. IPCSET Register Field Descriptions
Bit
Field
Type
Reset
Description
31
IPC31
R-0/W1S
0h
Writing 1 to this bit sets the IPC31 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
30
IPC30
R-0/W1S
0h
Writing 1 to this bit sets the IPC30 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
29
IPC29
R-0/W1S
0h
Writing 1 to this bit sets the IPC29 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
28
IPC28
R-0/W1S
0h
Writing 1 to this bit sets the IPC28 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
27
IPC27
R-0/W1S
0h
Writing 1 to this bit sets the IPC27 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
26
IPC26
R-0/W1S
0h
Writing 1 to this bit sets the IPC26 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
25
IPC25
R-0/W1S
0h
Writing 1 to this bit sets the IPC25 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
24
IPC24
R-0/W1S
0h
Writing 1 to this bit sets the IPC24 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
23
IPC23
R-0/W1S
0h
Writing 1 to this bit sets the IPC23 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
22
IPC22
R-0/W1S
0h
Writing 1 to this bit sets the IPC22 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
961
Copyright © 2024 Texas Instruments Incorporated
Table 7-27. IPCSET Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
21
IPC21
R-0/W1S
0h
Writing 1 to this bit sets the IPC21 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
20
IPC20
R-0/W1S
0h
Writing 1 to this bit sets the IPC20 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
19
IPC19
R-0/W1S
0h
Writing 1 to this bit sets the IPC19 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
18
IPC18
R-0/W1S
0h
Writing 1 to this bit sets the IPC18 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
17
IPC17
R-0/W1S
0h
Writing 1 to this bit sets the IPC17 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
16
IPC16
R-0/W1S
0h
Writing 1 to this bit sets the IPC16 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
15
IPC15
R-0/W1S
0h
Writing 1 to this bit sets the IPC15 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
14
IPC14
R-0/W1S
0h
Writing 1 to this bit sets the IPC14 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
13
IPC13
R-0/W1S
0h
Writing 1 to this bit sets the IPC13 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
12
IPC12
R-0/W1S
0h
Writing 1 to this bit sets the IPC12 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
11
IPC11
R-0/W1S
0h
Writing 1 to this bit sets the IPC11 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
10
IPC10
R-0/W1S
0h
Writing 1 to this bit sets the IPC10 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
9
IPC9
R-0/W1S
0h
Writing 1 to this bit sets the IPC9 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
8
IPC8
R-0/W1S
0h
Writing 1 to this bit sets the IPC8 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
7
IPC7
R-0/W1S
0h
Writing 1 to this bit sets the IPC7 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
6
IPC6
R-0/W1S
0h
Writing 1 to this bit sets the IPC6 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
5
IPC5
R-0/W1S
0h
Writing 1 to this bit sets the IPC5 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
962
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
Table 7-27. IPCSET Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
4
IPC4
R-0/W1S
0h
Writing 1 to this bit sets the IPC4 event flag for the remote CPU.
Writing 0 has no effect.
Reset type: SYSRSn
3
IPC3
R-0/W1S
0h
Writing 1 to this bit sets the IPC3 event flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
2
IPC2
R-0/W1S
0h
Writing 1 to this bit sets the IPC2 event flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
1
IPC1
R-0/W1S
0h
Writing 1 to this bit sets the IPC1 event flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
0
IPC0
R-0/W1S
0h
Writing 1 to this bit sets the IPC0 event flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
963
Copyright © 2024 Texas Instruments Incorporated
7.7.3.4 IPCCLR Register (Offset = 6h) [Reset = 00000000h] 
IPCCLR is shown in Figure 7-22 and described in Table 7-28.
Return to the Summary Table.
IPC remote flag clear register
Figure 7-22. IPCCLR Register
31
30
29
28
27
26
25
24
IPC31
IPC30
IPC29
IPC28
IPC27
IPC26
IPC25
IPC24
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
23
22
21
20
19
18
17
16
IPC23
IPC22
IPC21
IPC20
IPC19
IPC18
IPC17
IPC16
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
15
14
13
12
11
10
9
8
IPC15
IPC14
IPC13
IPC12
IPC11
IPC10
IPC9
IPC8
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
7
6
5
4
3
2
1
0
IPC7
IPC6
IPC5
IPC4
IPC3
IPC2
IPC1
IPC0
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
R-0/W1S-0h
Table 7-28. IPCCLR Register Field Descriptions
Bit
Field
Type
Reset
Description
31
IPC31
R-0/W1S
0h
Writing 1 to this bit clears the IPC31 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
30
IPC30
R-0/W1S
0h
Writing 1 to this bit clears the IPC30 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
29
IPC29
R-0/W1S
0h
Writing 1 to this bit clears the IPC29 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
28
IPC28
R-0/W1S
0h
Writing 1 to this bit clears the IPC28 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
964
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
Table 7-28. IPCCLR Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
27
IPC27
R-0/W1S
0h
Writing 1 to this bit clears the IPC27 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
26
IPC26
R-0/W1S
0h
Writing 1 to this bit clears the IPC26 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
25
IPC25
R-0/W1S
0h
Writing 1 to this bit clears the IPC25 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
24
IPC24
R-0/W1S
0h
Writing 1 to this bit clears the IPC24 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
23
IPC23
R-0/W1S
0h
Writing 1 to this bit clears the IPC23 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
22
IPC22
R-0/W1S
0h
Writing 1 to this bit clears the IPC22 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
21
IPC21
R-0/W1S
0h
Writing 1 to this bit clears the IPC21 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
20
IPC20
R-0/W1S
0h
Writing 1 to this bit clears the IPC20 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
965
Copyright © 2024 Texas Instruments Incorporated
Table 7-28. IPCCLR Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
19
IPC19
R-0/W1S
0h
Writing 1 to this bit clears the IPC19 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
18
IPC18
R-0/W1S
0h
Writing 1 to this bit clears the IPC18 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
17
IPC17
R-0/W1S
0h
Writing 1 to this bit clears the IPC17 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
16
IPC16
R-0/W1S
0h
Writing 1 to this bit clears the IPC16 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
15
IPC15
R-0/W1S
0h
Writing 1 to this bit clears the IPC15 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
14
IPC14
R-0/W1S
0h
Writing 1 to this bit clears the IPC14 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
13
IPC13
R-0/W1S
0h
Writing 1 to this bit clears the IPC13 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
12
IPC12
R-0/W1S
0h
Writing 1 to this bit clears the IPC12 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
966
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
Table 7-28. IPCCLR Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
11
IPC11
R-0/W1S
0h
Writing 1 to this bit clears the IPC11 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
10
IPC10
R-0/W1S
0h
Writing 1 to this bit clears the IPC10 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
9
IPC9
R-0/W1S
0h
Writing 1 to this bit clears the IPC9 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
8
IPC8
R-0/W1S
0h
Writing 1 to this bit clears the IPC8 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
7
IPC7
R-0/W1S
0h
Writing 1 to this bit clears the IPC7 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
6
IPC6
R-0/W1S
0h
Writing 1 to this bit clears the IPC6 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
5
IPC5
R-0/W1S
0h
Writing 1 to this bit clears the IPC5 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
4
IPC4
R-0/W1S
0h
Writing 1 to this bit clears the IPC4 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
967
Copyright © 2024 Texas Instruments Incorporated
Table 7-28. IPCCLR Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
3
IPC3
R-0/W1S
0h
Writing 1 to this bit clears the IPC3 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
2
IPC2
R-0/W1S
0h
Writing 1 to this bit clears the IPC2 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
1
IPC1
R-0/W1S
0h
Writing 1 to this bit clears the IPC1 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
0
IPC0
R-0/W1S
0h
Writing 1 to this bit clears the IPC0 flag for the remote CPU.
Writing 0 has no effect.
Notes:
[1] Normally, each CPU will clear (acknowledge) only its own local 
flags. This mechanism may be useful if the remote CPU is non-
responsive.
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
968
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
7.7.3.5 IPCFLG Register (Offset = 8h) [Reset = 00000000h] 
IPCFLG is shown in Figure 7-23 and described in Table 7-29.
Return to the Summary Table.
IPC remote flag status register
Figure 7-23. IPCFLG Register
31
30
29
28
27
26
25
24
IPC31
IPC30
IPC29
IPC28
IPC27
IPC26
IPC25
IPC24
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
23
22
21
20
19
18
17
16
IPC23
IPC22
IPC21
IPC20
IPC19
IPC18
IPC17
IPC16
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
15
14
13
12
11
10
9
8
IPC15
IPC14
IPC13
IPC12
IPC11
IPC10
IPC9
IPC8
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
7
6
5
4
3
2
1
0
IPC7
IPC6
IPC5
IPC4
IPC3
IPC2
IPC1
IPC0
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
R-0h
Table 7-29. IPCFLG Register Field Descriptions
Bit
Field
Type
Reset
Description
31
IPC31
R
0h
Indicates to the local CPU whether the remote IPC31 event flag is 
set.
0: The remote IPC31 event flag is not set
1: The remote IPC31 event flag is set
Reset type: SYSRSn
30
IPC30
R
0h
Indicates to the local CPU whether the remote IPC30 event flag is 
set.
0: The remote IPC30 event flag is not set
1: The remote IPC30 event flag is set
Reset type: SYSRSn
29
IPC29
R
0h
Indicates to the local CPU whether the remote IPC29 event flag is 
set.
0: The remote IPC29 event flag is not set
1: The remote IPC29 event flag is set
Reset type: SYSRSn
28
IPC28
R
0h
Indicates to the local CPU whether the remote IPC28 event flag is 
set.
0: The remote IPC28 event flag is not set
1: The remote IPC28 event flag is set
Reset type: SYSRSn
27
IPC27
R
0h
Indicates to the local CPU whether the remote IPC27 event flag is 
set.
0: The remote IPC27 event flag is not set
1: The remote IPC27 event flag is set
Reset type: SYSRSn
26
IPC26
R
0h
Indicates to the local CPU whether the remote IPC26 event flag is 
set.
0: The remote IPC26 event flag is not set
1: The remote IPC26 event flag is set
Reset type: SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
969
Copyright © 2024 Texas Instruments Incorporated
Table 7-29. IPCFLG Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
25
IPC25
R
0h
Indicates to the local CPU whether the remote IPC25 event flag is 
set.
0: The remote IPC25 event flag is not set
1: The remote IPC25 event flag is set
Reset type: SYSRSn
24
IPC24
R
0h
Indicates to the local CPU whether the remote IPC24 event flag is 
set.
0: The remote IPC24 event flag is not set
1: The remote IPC24 event flag is set
Reset type: SYSRSn
23
IPC23
R
0h
Indicates to the local CPU whether the remote IPC23 event flag is 
set.
0: The remote IPC23 event flag is not set
1: The remote IPC23 event flag is set
Reset type: SYSRSn
22
IPC22
R
0h
Indicates to the local CPU whether the remote IPC22 event flag is 
set.
0: The remote IPC22 event flag is not set
1: The remote IPC22 event flag is set
Reset type: SYSRSn
21
IPC21
R
0h
Indicates to the local CPU whether the remote IPC21 event flag is 
set.
0: The remote IPC21 event flag is not set
1: The remote IPC21 event flag is set
Reset type: SYSRSn
20
IPC20
R
0h
Indicates to the local CPU whether the remote IPC20 event flag is 
set.
0: The remote IPC20 event flag is not set
1: The remote IPC20 event flag is set
Reset type: SYSRSn
19
IPC19
R
0h
Indicates to the local CPU whether the remote IPC19 event flag is 
set.
0: The remote IPC19 event flag is not set
1: The remote IPC19 event flag is set
Reset type: SYSRSn
18
IPC18
R
0h
Indicates to the local CPU whether the remote IPC18 event flag is 
set.
0: The remote IPC18 event flag is not set
1: The remote IPC18 event flag is set
Reset type: SYSRSn
17
IPC17
R
0h
Indicates to the local CPU whether the remote IPC17 event flag is 
set.
0: The remote IPC17 event flag is not set
1: The remote IPC17 event flag is set
Reset type: SYSRSn
16
IPC16
R
0h
Indicates to the local CPU whether the remote IPC16 event flag is 
set.
0: The remote IPC16 event flag is not set
1: The remote IPC16 event flag is set
Reset type: SYSRSn
15
IPC15
R
0h
Indicates to the local CPU whether the remote IPC15 event flag is 
set.
0: The remote IPC15 event flag is not set
1: The remote IPC15 event flag is set
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
970
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
Table 7-29. IPCFLG Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
14
IPC14
R
0h
Indicates to the local CPU whether the remote IPC14 event flag is 
set.
0: The remote IPC14 event flag is not set
1: The remote IPC14 event flag is set
Reset type: SYSRSn
13
IPC13
R
0h
Indicates to the local CPU whether the remote IPC13 event flag is 
set.
0: The remote IPC13 event flag is not set
1: The remote IPC13 event flag is set
Reset type: SYSRSn
12
IPC12
R
0h
Indicates to the local CPU whether the remote IPC12 event flag is 
set.
0: The remote IPC12 event flag is not set
1: The remote IPC12 event flag is set
Reset type: SYSRSn
11
IPC11
R
0h
Indicates to the local CPU whether the remote IPC11 event flag is 
set.
0: The remote IPC11 event flag is not set
1: The remote IPC11 event flag is set
Reset type: SYSRSn
10
IPC10
R
0h
Indicates to the local CPU whether the remote IPC10 event flag is 
set.
0: The remote IPC10 event flag is not set
1: The remote IPC10 event flag is set
Reset type: SYSRSn
9
IPC9
R
0h
Indicates to the local CPU whether the remote IPC9 event flag is set.
0: The remote IPC9 event flag is not set
1: The remote IPC9 event flag is set
Reset type: SYSRSn
8
IPC8
R
0h
Indicates to the local CPU whether the remote IPC8 event flag is set.
0: The remote IPC8 event flag is not set
1: The remote IPC8 event flag is set
Reset type: SYSRSn
7
IPC7
R
0h
Indicates to the local CPU whether the remote IPC7 event flag is set.
0: The remote IPC7 event flag is not set
1: The remote IPC7 event flag is set
Reset type: SYSRSn
6
IPC6
R
0h
Indicates to the local CPU whether the remote IPC6 event flag is set.
0: The remote IPC6 event flag is not set
1: The remote IPC6 event flag is set
Reset type: SYSRSn
5
IPC5
R
0h
Indicates to the local CPU whether the remote IPC5 event flag is set.
0: The remote IPC5 event flag is not set
1: The remote IPC5 event flag is set
Reset type: SYSRSn
4
IPC4
R
0h
Indicates to the local CPU whether the remote IPC4 event flag is set.
0: The remote IPC4 event flag is not set
1: The remote IPC4 event flag is set
Reset type: SYSRSn
3
IPC3
R
0h
Indicates to the local CPU whether the remote IPC3 event flag is set.
0: The remote IPC3 event flag is not set
1: The remote IPC3 event flag is set
Notes:
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
971
Copyright © 2024 Texas Instruments Incorporated
Table 7-29. IPCFLG Register Field Descriptions (continued)
Bit
Field
Type
Reset
Description
2
IPC2
R
0h
Indicates to the local CPU whether the remote IPC2 event flag is set.
0: The remote IPC2 event flag is not set
1: The remote IPC2 event flag is set
Notes:
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
1
IPC1
R
0h
Indicates to the local CPU whether the remote IPC1 event flag is set.
0: The remote IPC1 event flag is not set
1: The remote IPC1 event flag is set
Notes:
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
0
IPC0
R
0h
Indicates to the local CPU whether the remote IPC0 event flag is set.
0: The remote IPC0 event flag is not set
1: The remote IPC0 event flag is set
Notes:
[1] IPC event flags 0-3 will trigger interrupts in the receiving CPU via 
the ePIE.
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
972
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
7.7.3.6 IPCCOUNTERL Register (Offset = Ch) [Reset = 00000000h] 
IPCCOUNTERL is shown in Figure 7-24 and described in Table 7-30.
Return to the Summary Table.
IPC Counter Low Register
Figure 7-24. IPCCOUNTERL Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
COUNT
R-0h
Table 7-30. IPCCOUNTERL Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
COUNT
R
0h
This is the lower 32-bits of free running 64 bit timestamp counter 
clocked by the PLLSYSCLK.
Reset type: CPU1.SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
973
Copyright © 2024 Texas Instruments Incorporated
7.7.3.7 IPCCOUNTERH Register (Offset = Eh) [Reset = 00000000h] 
IPCCOUNTERH is shown in Figure 7-25 and described in Table 7-31.
Return to the Summary Table.
IPC Counter High Register
Figure 7-25. IPCCOUNTERH Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
COUNT
R-0h
Table 7-31. IPCCOUNTERH Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
COUNT
R
0h
This is the upper 32-bits of free running 64 bit timestamp counter 
clocked by the PLLSYSCLK.
Reset type: CPU1.SYSRSn
Interprocessor Communication (IPC)
www.ti.com
974
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
7.7.3.8 IPCRECVCOM Register (Offset = 10h) [Reset = 00000000h] 
IPCRECVCOM is shown in Figure 7-26 and described in Table 7-32.
Return to the Summary Table.
Remote to Local IPC Command Register
Figure 7-26. IPCRECVCOM Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
COMMAND
R-0h
Table 7-32. IPCRECVCOM Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
COMMAND
R
0h
This is a general purpose register used to receive software-defined 
commands from the remote CPU. It can only be written by the 
remote CPU.
Notes
[1] The local CPU's IPCRECVCOM is the same physical register 
as the remote CPU's IPCSENDCOM, and is located at the same 
address in both CPUs.
[2] This register is reset by a SYRSn of the remote CPU
Reset type: CPUx.SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
975
Copyright © 2024 Texas Instruments Incorporated
7.7.3.9 IPCRECVADDR Register (Offset = 12h) [Reset = 00000000h] 
IPCRECVADDR is shown in Figure 7-27 and described in Table 7-33.
Return to the Summary Table.
Remote to Local IPC Address Register
Figure 7-27. IPCRECVADDR Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
ADDRESS
R-0h
Table 7-33. IPCRECVADDR Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
ADDRESS
R
0h
This is a general purpose register used to receive software-defined 
addresses from the remote CPU. It can only be written by the remote 
CPU.
Notes
[1] The local CPU's IPCRECVADDR is the same physical register 
as the remote CPU's IPCSENDADDR, and is located at the same 
address in both CPUs.
[2] This register is reset by a SYRSn of the remote CPU
Reset type: CPUx.SYSRSn
Interprocessor Communication (IPC)
www.ti.com
976
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
7.7.3.10 IPCRECVDATA Register (Offset = 14h) [Reset = 00000000h] 
IPCRECVDATA is shown in Figure 7-28 and described in Table 7-34.
Return to the Summary Table.
Remote to Local IPC Data Register
Figure 7-28. IPCRECVDATA Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
WDATA
R-0h
Table 7-34. IPCRECVDATA Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
WDATA
R
0h
This is a general purpose register used to receive software-defined 
data from the remote CPU. It can only be written by the remote CPU.
Notes
[1] The local CPU's IPCRECVDATA is the same physical register 
as the remote CPU's IPCSENDDATA, and is located at the same 
address in both CPUs.
[2] This register is reset by a SYRSn of the remote CPU
Reset type: CPUx.SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
977
Copyright © 2024 Texas Instruments Incorporated
7.7.3.11 IPCLOCALREPLY Register (Offset = 16h) [Reset = 00000000h] 
IPCLOCALREPLY is shown in Figure 7-29 and described in Table 7-35.
Return to the Summary Table.
Local to Remote IPC Reply Data Register
Figure 7-29. IPCLOCALREPLY Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
RDATA
R/W-0h
Table 7-35. IPCLOCALREPLY Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
RDATA
R/W
0h
This is a general purpose register used to send software-defined 
data to the remote CPU in response to a command. It can only be 
written by the local CPU.
Notes
[1] The local CPU's IPCLOCALREPLY is the same physical register 
as the remote CPU's IPCREMOTEREPLY, and is located at the 
same address in both CPUs.
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
978
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
7.7.3.12 IPCSENDCOM Register (Offset = 18h) [Reset = 00000000h] 
IPCSENDCOM is shown in Figure 7-30 and described in Table 7-36.
Return to the Summary Table.
Local to Remote IPC Command Register
Figure 7-30. IPCSENDCOM Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
COMMAND
R/W-0h
Table 7-36. IPCSENDCOM Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
COMMAND
R/W
0h
This is a general purpose register used to send software-defined 
commands to the remote CPU. It can only be written by the local 
CPU.
Notes
[1] The local CPU's IPCSENDCOM is the same physical register 
as the remote CPU's IPCRECVCOM, and is located at the same 
address in both CPUs.
Reset type: SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
979
Copyright © 2024 Texas Instruments Incorporated
7.7.3.13 IPCSENDADDR Register (Offset = 1Ah) [Reset = 00000000h] 
IPCSENDADDR is shown in Figure 7-31 and described in Table 7-37.
Return to the Summary Table.
Local to Remote IPC Address Register
Figure 7-31. IPCSENDADDR Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
ADDRESS
R/W-0h
Table 7-37. IPCSENDADDR Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
ADDRESS
R/W
0h
This is a general purpose register used to send software-defined 
addresses to the remote CPU. It can only be written by the local 
CPU.
Notes
[1] The local CPU's IPCSENDADDR is the same physical register 
as the remote CPU's IPCRECVDATA, and is located at the same 
address in both CPUs.
Reset type: SYSRSn
Interprocessor Communication (IPC)
www.ti.com
980
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
7.7.3.14 IPCSENDDATA Register (Offset = 1Ch) [Reset = 00000000h] 
IPCSENDDATA is shown in Figure 7-32 and described in Table 7-38.
Return to the Summary Table.
Local to Remote IPC Data Register
Figure 7-32. IPCSENDDATA Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
WDATA
R/W-0h
Table 7-38. IPCSENDDATA Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
WDATA
R/W
0h
This is a general purpose register used to send software-defined 
data to the remote CPU. It can only be written by the local CPU.
Notes
[1] The local CPU's IPCSENDDATA is the same physical register 
as the remote CPU's IPCRECVDATA, and is located at the same 
address in both CPUs.
Reset type: SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
981
Copyright © 2024 Texas Instruments Incorporated
7.7.3.15 IPCREMOTEREPLY Register (Offset = 1Eh) [Reset = 00000000h] 
IPCREMOTEREPLY is shown in Figure 7-33 and described in Table 7-39.
Return to the Summary Table.
Remote to Local IPC Reply Data Register
Figure 7-33. IPCREMOTEREPLY Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
RDATA
R-0h
Table 7-39. IPCREMOTEREPLY Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
RDATA
R
0h
This is a general purpose register used to receive software-defined 
data from the remote CPU's response to a command. It can only be 
written by the remote CPU.
Notes
[1] The local CPU's IPCREMOTEREPLY is the same physical 
register as the remote CPU's IPCLOCALREPLY, and is located at 
the same address in both CPUs.
[2] This register is reset by a SYRSn of the remote CPU
Reset type: CPUx.SYSRSn
Interprocessor Communication (IPC)
www.ti.com
982
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
7.7.3.16 IPCBOOTSTS Register (Offset = 20h) [Reset = 00000000h] 
IPCBOOTSTS is shown in Figure 7-34 and described in Table 7-40.
Return to the Summary Table.
CPU2 to CPU1 IPC Boot Status Register
Figure 7-34. IPCBOOTSTS Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
BOOTSTS
R/W-0h
Table 7-40. IPCBOOTSTS Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
BOOTSTS
R/W
0h
This register is used by CPU2 to pass the boot Status to CPU1. The 
data format is software-defined. It can only be written by CPU2.
Reset type: CPU2.SYSRSn
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
983
Copyright © 2024 Texas Instruments Incorporated
7.7.3.17 IPCBOOTMODE Register (Offset = 22h) [Reset = 00000000h] 
IPCBOOTMODE is shown in Figure 7-35 and described in Table 7-41.
Return to the Summary Table.
CPU1 to CPU2 IPC Boot Mode Register
Figure 7-35. IPCBOOTMODE Register
31
30
29
28
27
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
BOOTMODE
R/W-0h
Table 7-41. IPCBOOTMODE Register Field Descriptions
Bit
Field
Type
Reset
Description
31-0
BOOTMODE
R/W
0h
This register is used by CPU1 to pass a boot mode information to 
CPU2. The data format is software-defined. It can only be written by 
CPU1.
Reset type: CPU1.SYSRSn
7.7.4 IPC Registers to Driverlib Functions
Table 7-42. IPC Registers to Driverlib Functions
File
Driverlib Function
ACK
-
STS
-
SET
-
CLR
-
FLG
-
COUNTERL
-
COUNTERH
-
SENDCOM
-
SENDADDR
-
SENDDATA
-
REMOTEREPLY
-
RECVCOM
-
RECVADDR
-
RECVDATA
-
Interprocessor Communication (IPC)
www.ti.com
984
TMS320F2837xD Dual-Core Real-Time Microcontrollers
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated
Table 7-42. IPC Registers to Driverlib Functions (continued)
File
Driverlib Function
LOCALREPLY
-
RECVCOM
-
RECVADDR
-
RECVDATA
-
LOCALREPLY
-
SENDCOM
-
SENDADDR
-
SENDDATA
-
REMOTEREPLY
-
BOOTSTS
-
BOOTMODE
-
www.ti.com
Interprocessor Communication (IPC)
SPRUHM8K – DECEMBER 2013 – REVISED MAY 2024
Submit Document Feedback
TMS320F2837xD Dual-Core Real-Time Microcontrollers
985
Copyright © 2024 Texas Instruments Incorporated
