#emacs, this is -*-Python-*- mode
import sys, os, glob
#import subprocess

# run with "scons debug=1" to enable debug symbols

# On Windows, cam_iface could probably work with gcc, but hasn't been
# ported yet. (It works fine on linux.)

debug = int(ARGUMENTS.get('debug', 0))

HAVE_MANIFEST_TOOL=False
if sys.platform.startswith('win'):
    # check if mt.exe exists on path
    if 'MSVCDir' in os.environ: # VC7
        msvc_dir = os.environ['MSVCDir']
        versionname = os.path.split(msvc_dir)[-1]
        if not versionname == 'VC7':
            print 'WARNING: Could not determing if you have a manifest tool (mt.ext)'
    elif 'VCINSTALLDIR' in os.environ: # VC8
        HAVE_MANIFEST_TOOL=True

BUILD_BACKENDS = ['blank']
#BUILD_BACKENDS = []

class PrereqsNotFoundError(Exception): pass

def add_blank( d ):
    d.setdefault('source',[]).append('src/cam_iface_blank.c')

def add_system_raw1394( d ):
    d.setdefault('LIBS',[]).append('raw1394')
    
def add_dc1394v2( d ):
    if sys.platform.startswith('darwin'):
        static_build_default = True # include dc1394 code in extension
    else:
        static_build_default = False # link to dynamic library in extension
        
    # assume dc1394 is installed to prefix /usr
    dc1394_prefix = os.environ.get('DC1394_PREFIX','/usr')
    static_build = os.environ.get('CAMIFACE_DC1394_STATIC',static_build_default)

    if not static_build:
        include_dir = os.path.join(dc1394_prefix,'include') # e.g. /usr/include
    else:
        include_dir = dc1394_prefix
        
    if not os.path.exists(os.path.join(include_dir,'dc1394/control.h')):
        raise PrereqsNotFoundError('libdc1394 (v2) not found at prefix "%s" '
                                   '- refusing to build'%dc1394_prefix)
    
    if not static_build:
        d.setdefault('LIBS',[]).extend(['dc1394'])
    else:
        sources = glob.glob(os.path.join(dc1394_prefix,'dc1394/*.c'))
        if sys.platform.startswith('darwin'):
            sources = sources + glob.glob(os.path.join(dc1394_prefix,'dc1394/macosx/*.c'))
        elif sys.platform.startswith('linux'):
            sources = sources + glob.glob(os.path.join(dc1394_prefix,'dc1394/linux/*.c'))
        d.setdefault('source',[]).extend(sources)
        d.setdefault('CPPPATH',[]).extend( [dc1394_prefix,
                                        os.path.join(dc1394_prefix,'dc1394')] )
    
    if sys.platform.startswith('darwin'):
        d.setdefault('LINKFLAGS',[]).extend('-framework IOKit -framework CoreFoundation'.split())
    d.setdefault('LIBS',[]).extend(['m'])
    
def add_system_libdc1394( d ):
    if not os.path.exists('/usr/include/libdc1394/dc1394_control.h'):
        raise PrereqsNotFoundError('libdc1394 not in system headers - refusing to build')
    d.setdefault('LIBS',[]).append('dc1394_control')

def add_openthreads(d):    
    opj = os.path.join
    if 1:
        # system-installed OpenThreads
        if sys.platform.startswith('linux'):
            OSG_incs = []
            OSG_libs = []
            libs = []
        elif sys.platform.startswith('win'):
            OSG_root = r'C:\Program Files\OpenSceneGraph'
            OSG_incs = [opj(OSG_root,'include')]
            OSG_libs = [opj(OSG_root,'lib')]
            libs = ['OpenThreadsWin32.lib']
        d.setdefault('CPPPATH',[]).extend(OSG_incs)
        d.setdefault('LIBPATH',[]).extend(OSG_libs)
        d.setdefault('LIBS',[]).extend(libs)
    else:
        # build OpenThreads from source
        d.setdefault('CPPPATH',[]).extend([opj('OpenThreads','include')])

def add_prosilica_gige( d ):
    #add_openthreads(d)
    opj = os.path.join
    d.setdefault('source',[]).extend([opj('src','cam_iface_prosilica_gige.cpp'),
                                      ])
    d.setdefault('CPPPATH',[]).append(opj('Prosilica GigE SDK','inc-pc'))
    d.setdefault('LIBS',[]).extend(['PvAPI'])
    
    if sys.platform.startswith('linux'):
        # _LINUX is defined in Prosilica's examples
        d.setdefault('CPPDEFINES',{}).update( {'_LINUX':None} )
        d.setdefault('LIBS',[]).extend(['rt', # for clock_gettime(), timer_create(), timer_settime()
                                        'pthread',
                                        ])
	if os.uname()[4] in ['i686']:
	    d.setdefault('CPPDEFINES',{}).update( {'_x86':None} )
        # XXX this is for static linking with GCC 4.0 on x86 architecture
        d.setdefault('LIBPATH',[]).append( opj('Prosilica GigE SDK','lib-pc','x86','4.0'))

        d.setdefault('LIBS',[]).append('-lOpenThreads')
        
    elif sys.platform.startswith('win'):
        # _LINUX is defined in Prosilica's examples
        d.setdefault('CPPDEFINES',{}).update( {'_WINDOWS':None} )
        d.setdefault('LIBPATH',[]).append( opj('Prosilica GigE SDK','lib-pc'))
    
def add_camwire( d ):
    d.setdefault('source',[]).extend(['src/cam_iface_camwire.c',
                                      'camwire/src/camwire_1394.c',
                                      'camwire/src/camwirebus_1394.c',
                                      ])
    d.setdefault('CPPPATH',[]).append('camwire/src')
    add_system_raw1394(d)
    add_system_libdc1394(d)
    
def add_dc1394_backend( d ):
    d.setdefault('source',[]).extend(['src/cam_iface_dc1394.c',])
    add_dc1394v2( d )
    if not sys.platform.startswith('darwin'):
        add_system_raw1394( d )
    else:
        d.setdefault('CFLAGS',[]).append('-DDISABLE_TRIGGER_CODE')
        
def add_quicktime_backend( d ):
    d.setdefault('source',[]).extend(['src/cam_iface_quicktime.c',])
    #add_my_quicktime( d )
    if sys.platform == 'darwin':
        d.setdefault('LINKFLAGS',[]).extend('-framework QuickTime -framework Carbon'.split())
    
def add_imperx( d ):
    LYNXGIGE_ROOT = r'C:\Program Files\ImperX\LYNX GigE'

    if not os.path.exists(LYNXGIGE_ROOT):
        raise PrereqsNotFoundError('imperx not found')
    
    d.setdefault('source',[]).append('src/cam_iface_imperx.cpp')
    VCInstallDir = r'C:\Program Files\Microsoft Visual Studio 8\VC'
    d.setdefault('CPPPATH',[]).extend( [LYNXGIGE_ROOT+r'\Includes\stlport',
                          os.path.join(VCInstallDir,'include'),
                          LYNXGIGE_ROOT+r'\Includes',
                         ] )
    d.setdefault('LIBS',[]).extend(['CyCamImperxLynx',
                       'CyCamLib',
                       'CyComLib',
                       'CyImgLib',
                       'CyMediumLib',
                       'CyUtilsLib',])
    d.setdefault('LIBPATH',[]).append( LYNXGIGE_ROOT+r'\Libraries' )
    d.setdefault('CPPDEFINES',{}).update( {'WIN32':None,
                                '_VC80_UPGRADE':'0x0600',
                                '_CONSOLE':None,
                                '_MBCD':None,
                               })

def add_stuff( backend, cam_iface_obj_dict ):
    dispatch = {'blank':add_blank,
                'camwire':add_camwire,
                'dc1394':add_dc1394_backend,
                'quicktime':add_quicktime_backend,
                'imperx':add_imperx,
                'prosilica_gige':add_prosilica_gige}
    dispatch[backend](cam_iface_obj_dict)

if sys.platform.startswith('linux'):
    BUILD_BACKENDS += ['dc1394']
    BUILD_BACKENDS += ['camwire']
    if os.uname()[4]=='i686':
        # only builds on i386/i686 architectures for now...
        BUILD_BACKENDS += ['prosilica_gige']
elif sys.platform.startswith('win'):
    BUILD_BACKENDS += ['imperx']
    BUILD_BACKENDS += ['prosilica_gige']
if sys.platform.startswith('darwin'):
    BUILD_BACKENDS += ['dc1394']
    BUILD_BACKENDS += ['quicktime']

built_backends = []
for backend in BUILD_BACKENDS:
    env = Environment(ENV = os.environ)

    libname='cam_iface_%s'%backend
    #lib_output_dir = '.'
    lib_output_dir = 'cam_iface'
    cam_iface_external_libpath = ['#%s'%lib_output_dir]
    cam_iface_external_cpppath = []

    cam_iface_obj_dict = {}
    #cam_iface_obj_dict['CPPDEFINES']={'CAM_IFACE_DEBUG':None}
    #cam_iface_obj_dict['CFLAGS']=['-O2','-Wall','-ansi','-pedantic']
    
    if 1:
        # compile similarly to Python distutils
        cam_iface_obj_dict.setdefault('CPPDEFINES',{}).update({'NDEBUG':None})

        # if using MSVC (this isn't the right test)
        if sys.platform.startswith('win'):
            cam_iface_obj_dict['CPPDEFINES'].update({'CAM_IFACE_DLL':None,
                                                     'CAM_IFACE_EXPORTS':None,
                                                     })
            cam_iface_obj_dict.setdefault('CCFLAGS',[]).extend(
                ['/c',  # Compile Without Linking
                 '/nologo',
                 '/Ox', # Full Optimization
                 '/MD', # Use Run-Time Library
                 '/W3', # Warning Level
                 '/EHsc', # Enable Exception Handling (changed from /GX)
                 ])
            
        # if using GCC (this isn't the right test)
        if sys.platform.startswith('linux'):
            if debug:
                cam_iface_obj_dict.setdefault('CCFLAGS',[]).append('-g')

        # if using darwin/MacOS X
        if sys.platform.startswith('darwin'):
            # build universal binaries (see http://developer.apple.com/technotes/tn2005/tn2137.html)
            cam_iface_obj_dict.setdefault('CFLAGS',[]).extend(
                '-O -g -isysroot /Developer/SDKs/MacOSX10.4u.sdk -arch i386 -arch ppc'.split())
            cam_iface_obj_dict.setdefault('LDFLAGSS',[]).extend('-arch i386 -arch ppc'.split())
            
    cam_iface_obj_dict.setdefault('CPPPATH',[]).append('inc')

    cam_iface_obj_dict['target']=os.path.join(lib_output_dir,libname)

    try:
        add_stuff( backend, cam_iface_obj_dict )
    except PrereqsNotFoundError,err:
        print 'SKIPPING backend "%s" because of PrereqsNotFoundError: "%s"'%(
            backend,str(err))
        print
        continue
    
    built_backends.append(backend)
    
    if sys.platform.startswith('win'):
        if 1:
            # include directory for "windows.h"
            platform_sdk_dir = r'C:\Program Files\Microsoft Platform SDK for Windows Server 2003 R2'
            platform_sdk_inc_dir = os.path.join(platform_sdk_dir,'Include')
            cam_iface_obj_dict.setdefault('CPPPATH',[]).append(platform_sdk_inc_dir)
            cam_iface_external_cpppath.append( platform_sdk_inc_dir )

            platform_sdk_lib_dir = os.path.join(platform_sdk_dir,'Lib')
            cam_iface_obj_dict.setdefault('LIBPATH',[]).append( platform_sdk_lib_dir )
            cam_iface_external_libpath.append( platform_sdk_lib_dir )

        manifest = '%s.dll.manifest'%libname
        if HAVE_MANIFEST_TOOL:
            # generate manifest
            env['LINKFLAGS'] = [env['LINKFLAGS'], '/manifest']

            # use manifest tool (mt.exe) after link step for executables
            env['LINKCOM'] = [env['LINKCOM'], 
              'mt.exe /nologo /manifest %s /outputresource:$TARGET;1' % os.path.join(lib_output_dir,manifest)]

            # use manifest tool (mt.exe) after link step for shared libraries
            env['SHLINKCOM'] = [env['SHLINKCOM'], 
              'mt.exe /nologo /manifest %s /outputresource:$TARGET;#2' % os.path.join(lib_output_dir,manifest)]

    env.SharedLibrary(**cam_iface_obj_dict)

    # --------------------------------------
    #
    # Done building cam_iface
    #
    # --------------------------------------
    cam_iface_external_libs = [libname]

    cam_iface_env_kwargs = cam_iface_obj_dict['CPPDEFINES']
    Export('env','backend','cam_iface_external_libs',
           'cam_iface_external_libpath','cam_iface_external_cpppath',
           'cam_iface_env_kwargs')
    SConscript( 'demo/SConscript' )

# save built backends
if 1:
    # XXX Need to convert this to scons way of building products -- this file is not erased with "scons -c".
    mydir = '.'#os.path.split(__file__)[0]
    fname = os.path.join( mydir, 'ctypes_backends.txt')
    fd = open( fname, mode='wb')
    for backend in built_backends:
        fd.write( backend + '\n')
    fd.close()
