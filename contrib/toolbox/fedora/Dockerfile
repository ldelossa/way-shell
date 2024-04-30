FROM quay.io/toolbx-images/fedora-toolbox:40

RUN sudo dnf install -y \
    libadwaita-devel \
    upower-devel \
    wireplumber-devel \
    json-glib-devel \
    NetworkManager-libnm-devel \
    pulseaudio-libs-devel \
    meson \
    cmake \
    gtk-doc

# build and install gtk4-layer-shell v1.0.1
RUN cd /tmp && \
    curl -LO https://github.com/wmww/gtk4-layer-shell/archive/refs/tags/v1.0.2.tar.gz && \
    tar -xvf v1.0.2.tar.gz && cd gtk4-layer-shell-1.0.2  && \
    meson setup --prefix=/usr -Dexamples=true -Ddocs=true -Dtests=true build && \
    ninja -C build/ && \
    ninja -C build/ install && \
    rm -rf /tmp/v1.0.2.tar.gz \
    rm -rf gtk4-layer-shell-1.0.2

# clone and build way-shell and way-sh
RUN mkdir /src && \
    cd /src && \
    git clone https://github.com/ldelossa/way-shell.git && \
    cd way-shell && \
    git submodule init && git submodule update && \
    make && \
    make gschema && \
    cp way-shell /usr/local/bin/ && \
    cp way-sh/way-sh /usr/local/bin && \
    rm -rf /src

CMD ["/usr/local/bin/way-shell"]
