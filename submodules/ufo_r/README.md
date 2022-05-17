# UFO R

UFO R API.

**Warning:** UFOs are under active development. Some bugs are to be expected,
and some features are not yet fully implemented. 

## System requirements

- R 3.6.0 or higher
- Linux 4.3 or higher (we currently do not support other operating systems)


## Prerequisites

Check if your operating system restricts who can call `userfaultfd`:

```bash
cat /proc/sys/vm/unprivileged_userfaultfd
```

*0* means only privileged users can call `userfaultfd` and UFOs will only work
for privileged users. To allow unprivileged users to call `userfaultfd`:

```bash
sysctl -w vm.unprivileged_userfaultfd=1
```


## Building

Before building, retrieve the code of a submodule:

```bash
git submodule update --init --recursive
```

To update the submodule, pull it.

```bash
cd src/ufo_c && git pull origin main && cargo update && cd ../..
```

Install the package with R. This compiles and properly install the package.

```bash
R CMD INSTALL --preclean .
```

You can also build the project with debug symbols by setting (exporting) the `UFO_DEBUG` environmental variable to `1`.

```
UFO_DEBUG=1 R CMD INSTALL --preclean .
```


## Troubleshooting

### Operation not permitted

```
syscall/userfaultfd: Operation not permitted
error initializing User-Fault file descriptor: Invalid argument
Error: package or namespace load failed for ‘ufos’:
 .onLoad failed in loadNamespace() for 'ufos', details:
  call: .initialize()
  error: Error initializing the UFO framework (-1)
```

The user has insufficient privileges to execute a userfaultfd system call. 

One likely culprit is that a global `sysctl` knob `vm.unprivileged_userfaultfd` to
control whether userfaultfd is allowed by unprivileged users was added to kernel
settings. If `/proc/sys/vm/unprivileged_userfaultfd` is `0`, do:

```bash
sysctl -w vm.unprivileged_userfaultfd=1
```

### Div_floor

```
error[E0599]: no method named `div_floor` found for type `usize` in the current scope
   --> /home/kondziu/.cargo/git/checkouts/ufo-core-6fe53746510c8ee1/853284b/src/ufo_objects.rs:193:47
    |
193 |         let chunk_number = offset_from_header.div_floor(bytes_loaded_at_once);
    |                                               ^^^^^^^^^ method not found in `usize`
    |
   ::: /home/kondziu/.cargo/registry/src/github.com-1ecc6299db9ec823/num-integer-0.1.44/src/lib.rs:54:8
    |
54  |     fn div_floor(&self, other: &Self) -> Self;
    |        --------- the method is available for `usize` here
    |
    = help: items from traits can only be used if the trait is in scope
help: the following trait is implemented but not in scope; perhaps add a `use` for it:
    |
1   | use num::Integer;
    |
```

You are using an older version of the Rust compiler. Consider upgrading:

```
rustup upgrade
```
