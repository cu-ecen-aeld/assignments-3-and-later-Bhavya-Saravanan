#Faulty Kernel Oops — Assignment 7
##Oops log captured from QEMU


```

echo "hello_world" > /dev/faulty
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
  ESR = 0x0000000096000045
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
  FSC = 0x05: level 1 translation fault
Data abort info:
  ISV = 0, ISS = 0x00000045
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=0000000041b48000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 0000000096000045 [#2] SMP
Modules linked in: hello(O) faulty(O) scull(O)
CPU: 0 PID: 164 Comm: sh Tainted: G      D    O       6.1.44 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : faulty_write+0x10/0x20 [faulty]
lr : vfs_write+0xc8/0x390
sp : ffffffc008dabd20
x29: ffffffc008dabd80 x28: ffffff8001aa4f80 x27: 0000000000000000
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
x23: 000000000000000c x22: 000000000000000c x21: ffffffc008dabdc0
x20: 000000555c06ddd0 x19: ffffff8001bdea00 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
x5 : 0000000000000001 x4 : ffffffc000787000 x3 : ffffffc008dabdc0
x2 : 000000000000000c x1 : 0000000000000000 x0 : 0000000000000000
Call trace:
 faulty_write+0x10/0x20 [faulty]
 ksys_write+0x74/0x110
 __arm64_sys_write+0x1c/0x30
 invoke_syscall+0x54/0x130
 el0_svc_common.constprop.0+0x44/0xf0
 do_el0_svc+0x2c/0xc0
 el0_svc+0x2c/0x90
 el0t_64_sync_handler+0xf4/0x120
 el0t_64_sync+0x18c/0x190
Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
---[ end trace 0000000000000000 ]---
```
The crash happened inside faulty_write() at offset +0x10
Writing to /dev/faulty calls that buggy function, which intentionally writes to address 0x0, so the kernel reports an Oops.

##Disassembly (objdump) of faulty_write() and mapping to the Oops

**How I ran objdump (host machine):**

```

./buildroot/output/host/bin/aarch64-buildroot-linux-gnu-objdump -d \
  buildroot/output/target/lib/modules/*/extra/faulty.ko \
  | sed -n '/<faulty_write>/,/^$/p'
```

**Output:**

```

0000000000000000 <faulty_write>:
   0:  d2800001   mov  x1, #0x0                 // x1 = 0
   4:  d2800000   mov  x0, #0x0                 // x0 = 0
   8:  d503233f   paciasp
   c:  d50323bf   autiasp
  10:  b900003f   str  wzr, [x1]               // store zero to *x1  (x1 = 0x0)-CRASH HERE
  14:  d65f03c0   ret
```

The faulting instruction is at offset +0x10, which is attempting to write to the x1 register.
Since x1 = 0 (set earlier), this becomes a write to address 0x0, i.e., a NULL pointer dereference.So, exactly what the Oops reported.


