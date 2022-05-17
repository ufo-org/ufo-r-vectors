# UFO R Vectors

This package contains a collection of example implementations of lazily
generated, larger-than-memory vectors for R using the [R
API](https://github.com/ufo-org/ufo-r) of the [UFO
framework](https://github.com/ufo-org/ufo-core).

UFO R vectors are implemented with the Userfault Object (UFO) framework. These
vectors are indistinguishable from plain old R vectors and can be used with
existing R code. Moreover, unlike other R larger-than-memory frameworks which
use S3/S4 objects to mimick vectors, UFO R vectors are also *internally*
indistinguishable from ordinary R objects, making UFOs usable from within
existing C and C++ code.

UFO R vectors are generated *lazily*: when an element of the vector is accessed,
the UFO framework generates data for the vector using a *populate* function.
This function can read the data from an existing source, like a CSV file, a
binary file, or network storage, or generate the data on the fly, like a
sequence. 

In addition, UFO vectors are *larger than memory*. The vectors are internally
split into chunks. When an element of the vector is accessed, only the chunk
around that position is loaded into memory. When the loaded chunks in all UFO
vectors take up too much room in process memory, the UFO framework unloads
oldest chunks to reclaim memory. This happens in a way invisible to the
programmer using the vectors, who can therefore operate on these vectors as if
they fit into memory.

This package provides vector implementations for the following vectors:

* sequences: `ufo_integer_seq`, `ufo_numeric_seq`,
* file-backed binary vectors: `ufo_integer_bin`, `ufo_numeric_bin`,
  `ufo_logical_bin`, `ufo_complex_bin`, `ufo_raw_bin` 
* vectors backed from CSV files: `ufo_csv`
* empty vectors: `ufo_integer`, `ufo_numeric`, `ufo_logical`, `ufo_complex`,
  `ufo_raw`, `ufo_character`, `ufo_vector`

Additionally, the package provides operators and helper functions for UFO
vectors: 

 * unary operators: `-`, `+`, `!`
 * binary arithmetic operators: `*`, `+`, `-`, `%%`, `/`, `%/%`
 * comparison operators: `<`, `<=`, `>`, `>=`, `>`, `>=`, `==`, `!=`, `|`, `&`
 * subsetting operators: `[`, `[<-`

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

The package requires the `viewports` package which can be installed from GitHub:

```bash
git clone https://github.com/ufo-org/viewports.git
R CMD INSTALL viewports
```

Allternatively, from R:

```R
devtools::install_github("ufo-org/viewports")
```

## Building

Before building, retrievew the code of a submodule:

```bash
git submodule update --init --recursive
```

To update the submodule, pull it.

```bash
cd submodules/ufo_r && git pull origin main && cd ../..
```

Install the package with R. This compiles and properly install the package.

```bash
R CMD INSTALL --preclean .
```

You can also build the project with debug symbols by setting (exporting) the
`UFO_DEBUG` environmental variable to `1`.

```bash
UFO_DEBUG=1 R CMD INSTALL --preclean .
```

## Testing

Check the package and display all errors:

```bash
_R_CHECK_TESTS_NLINES_=0 R CMD check .
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