# rawls
Rawly and recursively list files on device.

I noticed plain <code>ls -lR</code> was taking very long to finish when used on big filesystems. And worse it caused kernel to start caching all that stuff - even to point using swap.<br>

Rawls fixes this. First is it blazing fast and processes ~4MiB of linux_dirent's at time. Second it issues hint to kernel to not cache any of the files we queried.
<br>Rawls output is also easily machine parseable since each line of output has full pathname.

To compile it just invoke gcc: `gcc -O2 -o rawls rawls.c`

---

Example output of `time ./rawls / >/tmp/rootdevfiles:`

```
skipping: /proc
skipping: /media/DataTwo
skipping: /media/Data
skipping: /tmp
skipping: /dev
skipping: /sys
skipping: /boot
skipping: /run
sudo ./rawls / > /tmp/fsdump.list  3,54s user 18,83s system 55% cpu 40,465 total
less /tmp/fsdump.list
5000252;regular;64;32;1640145586009151860;'/root/.nft.history'
4980755;regular;141;32;1680386620938939456;'/root/.bashrc'
...
```

We see from above output that rawls also stays on its start device and won't recurse into mounts.

As for comparison here is same filesystem listed with `ls -filR`

`sudo ls -filR /mnt > /tmp/rootdevfiles  13,31s user 36,43s system 71% cpu 1:09,14 total`

ls was whole 17 seconds slower!!
