# Initializes the UFO framework
# Set high and low water marks by:
#    options(ufos.high_water_mark_mb = 2048)
#    options(ufos.low_water_mark_mb = 1024)
.initialize <- function(...) {
  high_water_mark_mb = as.integer(getOption("ufos.high_water_mark_mb", default=2048))
  low_water_mark_mb = as.integer(getOption("ufos.low_water_mark_mb", default=1024))
  invisible(.Call("ufo_initialize", high_water_mark_mb, low_water_mark_mb))
}

# Kills the UFO framework.
.jeff_goldbloom <- function(...) invisible(.Call("ufo_shutdown"))

# Finalizer for the entire UFO framework, called on session exit.
.onLoad <- function(libname, pkgname) {
  invisible(.initialize())
  invisible(reg.finalizer(.GlobalEnv, .jeff_goldbloom, onexit = TRUE))
}

# Finalizer for the entire UFO framework, called on package unload.
# AFAIK this cannot be relied on to be called on shutdown.
.onUnload <- function(libname, pkgname) {
  .jeff_goldbloom()
}

# Checks whether a vector is a UFO.
is_ufo <- function(x) {
	.Call("is_ufo", x)
}