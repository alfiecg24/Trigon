# Trigon

Trigon is a deterministic kernel exploit based on CVE-2023-32434. It currently supports A9 - A11 devices running iOS 13 - 15.7.6 . Being deterministic means that this exploit will never panic during or after exploitation and is completely reliable.

Trigon exploits an integer overflow in the VM layer of XNU. The vulnerability allows us to build an arbitrary physical address mapping primitive. This gives us read/write primitives to any physical address **unless it's a page table**. Not being able to read page tables made exploitation more difficult, but in the end we found some nice tricks to get around this issue and were able to build full virtual read/write primitives.

The full writeup can be found [here](https://alfiecg.uk/2025/03/01/Trigon.html). If you're into technical iOS-related writeups, I would recommend you take a read! I have tried to make it as understandable as possible so that those who are not iOS researchers can follow it too.