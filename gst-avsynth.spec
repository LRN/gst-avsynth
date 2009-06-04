%define majorminor  0.10
%define gstreamer   gstreamer
%define gst_minver  0.10.0
%define gst_majorminor  0.10


Name: 		%{gstreamer}-avsynth
Version: 	0.10.0.0
Release:	1	
Summary: 	GStreamer Streaming-media framework plugin that wraps AVSynth filters.

Group: 		Libraries/Multimedia
License: 	GPL
URL:		http://gstreamer.net/
Vendor:		GStreamer Backpackers Team <package@gstreamer.net>
Source:		http://gstreamer.freedesktop.org/src/gst-avsynth/gst-avsynth/gst-avsynth-%{version}.tar.gz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

Requires:  	%{gstreamer} >= 0.9.7
BuildRequires: 	%{gstreamer}-devel >= 0.9.7
BuildRequires: 	%{gstreamer}-plugins-base-devel >= 0.9.7
BuildRequires: 	liboil-devel >= 0.3.6

%description
GStreamer is a streaming-media framework, based on graphs of filters which
operate on media data. Applications using this library can do anything
from real-time sound processing to playing videos, and just about anything
else media-related. Its plugin-based architecture means that new data
types or processing capabilities can be added simply by installing new
plug-ins.

This plugin contains the plugin that wraps around AviSynth filters.

%prep
%setup -q -n gst-avsynth-%{version}

%build
%configure

make

%install
rm -rf $RPM_BUILD_ROOT

%makeinstall
rm -f $RPM_BUILD_ROOT%{_libdir}/gstreamer-%{gst_majorminor}/*.la
rm -f $RPM_BUILD_ROOT%{_libdir}/gstreamer-%{gst_majorminor}/*.a

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-, root, root, -)
%doc AUTHORS COPYING README gst-avsynth.doap

%changelog
* Fri Dec 15 2006 Thomas Vander Stichele <thomas at apestaart dot org>
- clean up
- add doap file

* Wed Oct 05 2004 Christian Schaller <christian at fluendo dot com>
- Update SPEC file to fit the times

* Sat Feb 14 2004 Thomas Vander Stichele <thomas at apestaart dot org>
- Clean up spec file

* Wed Jan 21 2004 Christian Schaller <Uraeus@gnome.org>
- First version of spec
