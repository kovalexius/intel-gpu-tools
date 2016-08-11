FROM build.netup:5000/iptv_2.0_build:2016-07-20

RUN apt-get install -y --force-yes aptitude
RUN apt-get install -y --force-yes less
RUN apt-get install -y --force-yes ocl-icd-opencl-dev

RUN apt-get install -y --force-yes autotools-dev
RUN apt-get install -y --force-yes autoconf
RUN apt-get install -y --force-yes xutils-dev
RUN apt-get install -y --force-yes libtool

## Trying to install newest package from backports
# ATTENTION!! Fix it!
RUN  cp /etc/apt/sources.list /etc/apt/sources.list.bak                                          &&    \
     echo "deb http://ftp.debian.org/debian jessie-backports main">>/etc/apt/sources.list
RUN  apt-get update            &&   \
     apt-get  -t  jessie-backports  install  -y  --force-yes  libdrm-dev
RUN  rm -f /etc/apt/sources.list && mv /etc/apt/sources.list.bak /etc/apt/sources.list
RUN  apt-get update

RUN apt-get install -y --force-yes libpciaccess-dev
RUN apt-get install -y --force-yes libcairo2-dev
RUN apt-get install -y --force-yes libunwind8-dev
RUN apt-get install -y --force-yes libfcgi-dev
