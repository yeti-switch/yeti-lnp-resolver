[![Build Status](https://github.com/yeti-switch/yeti-lnp-resolver/actions/workflows/main.yml/badge.svg)](https://github.com/yeti-switch/yeti-lnp-resolver/actions/workflows/main.yml)
[![Made in Ukraine](https://img.shields.io/badge/made_in-ukraine-ffd700.svg?labelColor=0057b7)](https://stand-with-ukraine.pp.ua)

[![Stand With Ukraine](https://raw.githubusercontent.com/vshymanskyy/StandWithUkraine/main/banner-direct-team.svg)](https://stand-with-ukraine.pp.ua)


# yeti-lnp-resolver

yeti-lnp-resolver is a part of the project [Yeti]

## Installation via Package (Debian 12/bookworm)
```sh
# apt install curl gnupg

# echo "deb [arch=amd64] http://apt.postgresql.org/pub/repos/apt bookworm-pgdg main" > /etc/apt/sources.list.d/pgdg.list
# curl https://www.postgresql.org/media/keys/ACCC4CF8.asc | gpg --dearmor > /etc/apt/trusted.gpg.d/apt.postgresql.org.gpg

# echo "deb [arch=amd64] http://pkg.yeti-switch.org/debian/bookworm 1.13 main" > /etc/apt/sources.list.d/yeti.list
# curl https://pkg.yeti-switch.org/key.gpg | gpg --dearmor > /etc/apt/trusted.gpg.d/pkg.yeti-switch.org.gpg

# apt update
# apt install sems-modules-yeti
```

## Building from sources (Debian 12+)

### get sources
```sh
$ git clone  --recursive https://github.com/yeti-switch/yeti-lnp-resolver.git
$ cd yeti-lnp-resolver
```

### install build dependencies
```sh
# apt build-dep .
```

### build package
```sh
$ dpkg-buildpackage -us -uc -b -j$(nproc)
```

[Yeti]:http://yeti-switch.org/
