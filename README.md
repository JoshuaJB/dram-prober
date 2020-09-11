# Software Side of DRAM Bank Address Bit Probing
After connecting the logic analyzer to the physical bank selection pins, use
these instructions to setup an environment to probe specified addresses. By
observing the bank selection pins when known addresses are accessed, one can
reverse engineer the boolean function used to determine the bank selection.

## Setup
1. Make sure that huge pages were enabled with `default_hugepagesz=1GB hugepagesz=1GB hugepages=1` on the kernel cmdline
2. Start `prober`
3. Use `/proc/<pid>/maps` to get virtual address of huge page
4. Use `pfn_q` with pid and virtual address to get page frame number
5. Convert hex page frame number to hex address by adding 3 zeros (multiply by 0x1000)
6. Run `echo "base=<frame address as hex> size=0x40000000 type=uncachable" >| /proc/mtrr`
7. Run `prober` program and observe activity on the DRAM bus with logic analyzer

## Additional Notes
If you run out of MMTRS (there are only 8) disable unused ones with
`echo "disable=<n>" >| /proc/mtrr` where `<n>` is the zero-base MMTR number
that you want to deconfigure.

## Physical Setup
On the shorter half of the physical DRAM slot, the pin assignments are as
follows:

Each `<` or `>` represents a pin location on one side, and "Key" is the
seperator between the longer and shorter halves of the slot.

```      _
Key     < >
        <_>
Event_n <_> PARITY
A0      <_> VDD
VDD     <_> BA1
BA0     <_>
```

Attach the logic analyzer probes to BA1 and BA0 and use any ground available.
The front panel connector ground pins seem to work fine.

## BIOS Configuration
Open "AMD Overclocking" and set the memory speed as low as possible.

## References
See JEDEC Standard JESD79-4B for the DDR4 SDRAM standard.
See Micron MTA9ASF51272PZ - 4GB datasheet for DDR4 SDRAM pinout.
