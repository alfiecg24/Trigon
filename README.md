# Trigon

Trigon is a deterministic kernel exploit based on CVE-2023-32434. It currently supports A10(X) devices running iOS 13 - 15.7.6 . Being deterministic means that this exploit will never panic during or after exploitation and is completely reliable.

In the future, I would like to add support for iOS 16.0 - 16.5, as well as expand the range of support chipsets. However, as the writeup explains, this is not always feasible.

Trigon exploits the same CVE as the one used in kfd's Smith exploit, except not as a physical use-after-free. Instead, it takes a different code path in the kernel and uses the vulnerability to create an arbitrary physical address mapping primitive. This gives us read/write primitives to any physical address **unless it's a page table**. Not being able to read page tables made exploitation more difficult, but in the end we found a nice trick to determine whether or not a page holds a page table before reading it and were able to build full virtual read/write primitives.

The full writeup can be found [here](https://alfiecg.uk/2025/03/01/Trigon.html). If you're into technical iOS-related writeups, I would recommend you take a read! I have tried to make it as understandable as possible so that those who are not iOS researchers can follow it too.