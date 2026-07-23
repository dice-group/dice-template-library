# Pointer Tagging

## x86-64 Pointer layout

### Sources
- https://docs.kernel.org/arch/x86/x86_64/mm.html

### Linux Normal (4-Level Paging)
```
MSB                                                            LSB
┣━━━━━━━━━━━━━━━━━━━━━━━━ Pointer (64 bit) ━━━━━━━━━━━━━━━━━━━━━━━┫
├─────────────────┤├────────────── Address (48 bit) ──────────────┤
 ^
 |
 sign-extension bits (16 bits)
    Can be used as free tagging bits, but must be restored to be copies of bit 47 before deref.
    For user-space addresses, bit 47 is always zero, therefore the rest of the bits must also be zero.
```

### Linux with AMD EPYC/Xeon Ice Lake (5-Level Paging)
```
MSB                                                            LSB
┣━━━━━━━━━━━━━━━━━━━━━━━━ Pointer (64 bit) ━━━━━━━━━━━━━━━━━━━━━━━┫
├──────┤├─────────────────── Address (57 bit) ────────────────────┤
 ^
 |
 sign-extension bits (7 bits)
    Can be used as free tagging bits, but must be restored to be copyies of bit 56 before deref.
    For user-space addresses, bit 56 is always zero, therefore the rest of the bits must also be zero.
```

### Additional Notes
Look out for `LAM` (Linear Address Masking, Intel)  / `UAI` (Upper Address Ignore, AMD) support in the future.
They are similar to `TBI` (Top Byte Ignore, ARM), but are not yet widely supported/enabled.



## aarch64 Pointer layout

### Sources
- https://docs.kernel.org/6.0/arm64/memory.html
- https://docs.kernel.org/6.0/arm64/memory-tagging-extension.html
- https://docs.kernel.org/6.0/arm64/tagged-address-abi.html
- Could not find official documentation from apple.

### Linux Normal (4KiB pages, 48 Bit Address Space)
```
MSB                                                            LSB
┣━━━━━━━━━━━━━━━━━━━━━━━━ Pointer (64 bit) ━━━━━━━━━━━━━━━━━━━━━━━┫
├───┤├───┤├───────┤├────────────── Address (48 bit) ──────────────┤
 ^    ^    ^
 |    |    |
 |    |    translation regimen selector (8 bit) (similar to sign-extension bits)
 |    |
 |    MTE (Memory Tagging Extension) bits (4 bit)
 |      Only if enabled (export GLIBC_TUNABLES=glibc.mem.tagging=1).
 |    
 free tagging bits (4 bit)
   Do not have to be restored because Top Byte Ignore (TBI) is always enabled in user space.
```

### Linux with Large Virtual Addresses (64KiB Pages, 52 Bit Address Space)
```
MSB                                                            LSB
┣━━━━━━━━━━━━━━━━━━━━━━━━ Pointer (64 bit) ━━━━━━━━━━━━━━━━━━━━━━━┫
├───┤├───┤├───┤├─────────────── Address (52 bit) ─────────────────┤
 ^    ^    ^
 |    |    |
 |    |    translation regimen selector (4 bit) (similar to sign-extension bits)
 |    |
 |    MTE (Memory Tagging Extension) bits (4 bit)
 |      only if enabled (export GLIBC_TUNABLES=glibc.mem.tagging=1)
 |    
 free tagging bits (4 bit)
   Do not have to be restored because Top Byte Ignore (TBI) is always enabled in user space.
```

### Apple Silicon (16KiB Pages, 47 Bit Address Space)
```
MSB                                                            LSB
┣━━━━━━━━━━━━━━━━━━━━━━━━ Pointer (64 bit) ━━━━━━━━━━━━━━━━━━━━━━━┫
├────────┤├────────┤├───────────── Address (47 bit) ──────────────┤
 ^         ^
 |         |
 |         translation regimen selector / additional metadata (9 bit)
 |    
 free tagging bits (8 bit)
    No MTE hardware present on Apple Silicon.
    Do not have to be restored because Top Byte Ignore (TBI) is enabled in user space.
```

## Conclusion
- x86-64 Linux has at most **7** high-bits safe to use for tagging
    - they must always be restored to their original value (copies of MSB of the actual address; all zeroes in user-space)
- aarch64 Linux has at most **4** high-bits safe to use for tagging
    - they don't have to be restored to their original value because of TBI
    - technically, an additional **4** bits could be used in the translation regimen selector, but they must be restored correctly (all zeros in user-space)
- aarch64 Darwin has at most **8** high-bits safe to use for tagging
    - they don't have to be restored to their original value because of TBI
    - we can not use additional bits in the translation regimen selector because darwin already uses them to store other metadata,
      and there is no way to restore the metadata without storing it somewhere (the bits are generally not just copies of bit 46)

- A safe choice is **4** high-bits.
- But we could get away with at most **7** high-bits across all architectures by using all available bits. On aarch64 Linux
  I would consider this somewhat risky because we need to use part of the translation regimen selector, which might get
  shrunken in the future to make space for larger addresses or for metadata like apple does it.

- We can also still tag the low-bits depending on type/pointer alignment. An alignment of 16 gives us an extra **4** low-bits for tagging.
  For a total of **8** safe-to-use tagging bits.
