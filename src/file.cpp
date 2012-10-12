#include <sstream>
#include <fstream>
#include <string>
#include <map>
#include <algorithm>
#include <locale>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/filesystem.hpp>
#include <boost/cstdint.hpp>
#include <boost/bind.hpp>
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include <cryptopp/md4.h>
#include <cassert>
#include "libed2k/constants.hpp"
#include "libed2k/log.hpp"
#include "libed2k/md4_hash.hpp"
#include "libed2k/file.hpp"

namespace libed2k
{

    using boost::uintmax_t;

    typedef std::map<std::string, EED2KFileType> SED2KFileTypeMap;
    typedef SED2KFileTypeMap::value_type SED2KFileTypeMapElement;
    static SED2KFileTypeMap ED2KFileTypesMap;


    class CED2KFileTypes{
    public:
        CED2KFileTypes()
        {
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".669"),   ED2KFT_AUDIO));      // 8 channel tracker module
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".aac"),   ED2KFT_AUDIO));      // Advanced Audio Coding File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".ac3"),   ED2KFT_AUDIO));      // Audio Codec 3 File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".aif"),   ED2KFT_AUDIO));      // Audio Interchange File Format
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".aifc"),  ED2KFT_AUDIO));      // Audio Interchange File Format
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".aiff"),  ED2KFT_AUDIO));      // Audio Interchange File Format
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".amf"),   ED2KFT_AUDIO));      // DSMI Advanced Module Format
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".amr"),   ED2KFT_AUDIO));      // Adaptive Multi-Rate Codec File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".ams"),   ED2KFT_AUDIO));      // Extreme Tracker Module
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".ape"),   ED2KFT_AUDIO));      // Monkey's Audio Lossless Audio File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".au"),    ED2KFT_AUDIO));      // Audio File (Sun, Unix)
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".aud"),   ED2KFT_AUDIO));      // General Audio File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".audio"), ED2KFT_AUDIO));      // General Audio File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".cda"),   ED2KFT_AUDIO));      // CD Audio Track
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".dbm"),   ED2KFT_AUDIO));
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".dmf"),   ED2KFT_AUDIO));      // Delusion Digital Music File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".dsm"),   ED2KFT_AUDIO));      // Digital Sound Module
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".dts"),   ED2KFT_AUDIO));      // DTS Encoded Audio File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".far"),   ED2KFT_AUDIO));      // Farandole Composer Module
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".flac"),  ED2KFT_AUDIO));      // Free Lossless Audio Codec File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".it"),    ED2KFT_AUDIO));      // Impulse Tracker Module
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".m1a"),   ED2KFT_AUDIO));      // MPEG-1 Audio File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".m2a"),   ED2KFT_AUDIO));      // MPEG-2 Audio File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".m4a"),   ED2KFT_AUDIO));      // MPEG-4 Audio File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mdl"),   ED2KFT_AUDIO));      // DigiTrakker Module
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".med"),   ED2KFT_AUDIO));      // Amiga MED Sound File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mid"),   ED2KFT_AUDIO));      // MIDI File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".midi"),  ED2KFT_AUDIO));      // MIDI File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mka"),   ED2KFT_AUDIO));      // Matroska Audio File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mod"),   ED2KFT_AUDIO));      // Amiga Music Module File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mol"),   ED2KFT_AUDIO));
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mp1"),   ED2KFT_AUDIO));      // MPEG-1 Audio File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mp2"),   ED2KFT_AUDIO));      // MPEG-2 Audio File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mp3"),   ED2KFT_AUDIO));      // MPEG-3 Audio File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mpa"),   ED2KFT_AUDIO));      // MPEG Audio File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mpc"),   ED2KFT_AUDIO));      // Musepack Compressed Audio File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mpp"),   ED2KFT_AUDIO));
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mtm"),   ED2KFT_AUDIO));      // MultiTracker Module
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".nst"),   ED2KFT_AUDIO));      // NoiseTracker
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".ogg"),   ED2KFT_AUDIO));      // Ogg Vorbis Compressed Audio File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".okt"),   ED2KFT_AUDIO));      // Oktalyzer Module (Amiga)
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".psm"),   ED2KFT_AUDIO));      // Protracker Studio Module
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".ptm"),   ED2KFT_AUDIO));      // PolyTracker Module
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".ra"),    ED2KFT_AUDIO));      // Real Audio File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".rmi"),   ED2KFT_AUDIO));      // MIDI File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".s3m"),   ED2KFT_AUDIO));      // Scream Tracker 3 Module
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".snd"),   ED2KFT_AUDIO));      // Audio File (Sun, Unix)
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".stm"),   ED2KFT_AUDIO));      // Scream Tracker 2 Module
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".ult"),   ED2KFT_AUDIO));      // UltraTracker
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".umx"),   ED2KFT_AUDIO));      // Unreal Music Package
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".wav"),   ED2KFT_AUDIO));      // WAVE Audio File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".wma"),   ED2KFT_AUDIO));      // Windows Media Audio File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".wow"),   ED2KFT_AUDIO));      // Grave Composer audio tracker
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".xm"),    ED2KFT_AUDIO));      // Fasttracker 2 Extended Module

            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".3g2"),   ED2KFT_VIDEO));      // 3GPP Multimedia File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".3gp"),   ED2KFT_VIDEO));      // 3GPP Multimedia File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".3gp2"),  ED2KFT_VIDEO));      // 3GPP Multimedia File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".3gpp"),  ED2KFT_VIDEO));      // 3GPP Multimedia File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".asf"),   ED2KFT_VIDEO));      // Advanced Systems Format (MS)
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".amv"),   ED2KFT_VIDEO));      // Anime Music Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".asf"),   ED2KFT_VIDEO));      // Advanced Systems Format File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".avi"),   ED2KFT_VIDEO));      // Audio Video Interleave File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".bik"),   ED2KFT_VIDEO));      // BINK Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".divx"),  ED2KFT_VIDEO));      // DivX-Encoded Movie File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".dvr-ms"),ED2KFT_VIDEO));      // Microsoft Digital Video Recording
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".flc"),   ED2KFT_VIDEO));      // FLIC Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".fli"),   ED2KFT_VIDEO));      // FLIC Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".flic"),  ED2KFT_VIDEO));      // FLIC Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".flv"),   ED2KFT_VIDEO));      // Flash Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".hdmov"), ED2KFT_VIDEO));      // High-Definition QuickTime Movie
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".ifo"),   ED2KFT_VIDEO));      // DVD-Video Disc Information File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".m1v"),   ED2KFT_VIDEO));      // MPEG-1 Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".m2t"),   ED2KFT_VIDEO));      // MPEG-2 Video Transport Stream
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".m2ts"),  ED2KFT_VIDEO));      // MPEG-2 Video Transport Stream
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".m2v"),   ED2KFT_VIDEO));      // MPEG-2 Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".m4b"),   ED2KFT_VIDEO));      // MPEG-4 Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".m4v"),   ED2KFT_VIDEO));      // MPEG-4 Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mkv"),   ED2KFT_VIDEO));      // Matroska Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mov"),   ED2KFT_VIDEO));      // QuickTime Movie File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".movie"), ED2KFT_VIDEO));      // QuickTime Movie File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mp1v"),  ED2KFT_VIDEO));      // QuickTime Movie File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mp2v"),  ED2KFT_VIDEO));      // MPEG-1 Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mp4"),   ED2KFT_VIDEO));      // MPEG-2 Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mpe"),   ED2KFT_VIDEO));      // MPEG-4 Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mpeg"),  ED2KFT_VIDEO));      // MPEG Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mpg"),   ED2KFT_VIDEO));      // MPEG Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mps"),   ED2KFT_VIDEO));      // MPEG Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mpv"),   ED2KFT_VIDEO));      // MPEG Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mpv1"),  ED2KFT_VIDEO));      // MPEG-1 Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mpv2"),  ED2KFT_VIDEO));      // MPEG-2 Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".ogm"),   ED2KFT_VIDEO));      // Ogg Media File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".ogv"),   ED2KFT_VIDEO));      // Ogg Theora Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".pva"),   ED2KFT_VIDEO));      // MPEG Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".qt"),    ED2KFT_VIDEO));      // QuickTime Movie
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".ram"),   ED2KFT_VIDEO));      // Real Audio Media
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".ratdvd"),ED2KFT_VIDEO));      // RatDVD Disk Image
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".rm"),    ED2KFT_VIDEO));      // Real Media File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".rmm"),   ED2KFT_VIDEO));      // Real Media File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".rmvb"),  ED2KFT_VIDEO));      // Real Video Variable Bit Rate File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".rv"),    ED2KFT_VIDEO));      // Real Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".rv9"),   ED2KFT_VIDEO));
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".smil"),  ED2KFT_VIDEO));      // SMIL Presentation File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".smk"),   ED2KFT_VIDEO));      // Smacker Compressed Movie File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".swf"),   ED2KFT_VIDEO));      // Macromedia Flash Movie
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".tp"),    ED2KFT_VIDEO));      // Video Transport Stream File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".ts"),    ED2KFT_VIDEO));      // Video Transport Stream File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".vid"),   ED2KFT_VIDEO));      // General Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".video"), ED2KFT_VIDEO));      // General Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".vivo"),  ED2KFT_VIDEO));      // VivoActive Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".vob"),   ED2KFT_VIDEO));      // DVD Video Object File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".vp6"),   ED2KFT_VIDEO));      // TrueMotion VP6 Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".webm"),  ED2KFT_VIDEO));      // WebM Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".wm"),    ED2KFT_VIDEO));      // Windows Media Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".wmv"),   ED2KFT_VIDEO));      // Windows Media Video File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".xvid"),  ED2KFT_VIDEO));      // Xvid-Encoded Video File

            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".bmp"),   ED2KFT_IMAGE));      // Bitmap Image File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".dcx"),   ED2KFT_IMAGE));      // FAXserve Fax Document
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".emf"),   ED2KFT_IMAGE));      // Enhanced Windows Metafile
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".gif"),   ED2KFT_IMAGE));      // Graphical Interchange Format File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".ico"),   ED2KFT_IMAGE));      // Icon File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".jfif"),  ED2KFT_IMAGE));      // JPEG File Interchange Format
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".jpe"),   ED2KFT_IMAGE));      // JPEG Image File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".jpeg"),  ED2KFT_IMAGE));      // JPEG Image File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".jpg"),   ED2KFT_IMAGE));      // JPEG Image File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".pct"),   ED2KFT_IMAGE));      // PICT Picture File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".pcx"),   ED2KFT_IMAGE));      // Paintbrush Bitmap Image File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".pic"),   ED2KFT_IMAGE));      // PICT Picture File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".pict"),  ED2KFT_IMAGE));      // PICT Picture File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".png"),   ED2KFT_IMAGE));      // Portable Network Graphic
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".psd"),   ED2KFT_IMAGE));      // Photoshop Document
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".psp"),   ED2KFT_IMAGE));      // Paint Shop Pro Image File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".tga"),   ED2KFT_IMAGE));      // Targa Graphic
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".tif"),   ED2KFT_IMAGE));      // Tagged Image File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".tiff"),  ED2KFT_IMAGE));      // Tagged Image File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".wbmp"),  ED2KFT_IMAGE));      // Wireless Application Protocol Bitmap Format
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".webp"),  ED2KFT_IMAGE));      // Weppy Photo File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".wmf"),   ED2KFT_IMAGE));      // Windows Metafile
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".wmp"),   ED2KFT_IMAGE));      // Windows Media Photo File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".xif"),   ED2KFT_IMAGE));      // ScanSoft Pagis Extended Image Format File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".xpm"),   ED2KFT_IMAGE));      // X-Windows Pixmap

            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".7z"),    ED2KFT_ARCHIVE));    // 7-Zip Compressed File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".ace"),   ED2KFT_ARCHIVE));    // WinAce Compressed File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".alz"),   ED2KFT_ARCHIVE));    // ALZip Archive
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".arc"),   ED2KFT_ARCHIVE));    // Compressed File Archive
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".arj"),   ED2KFT_ARCHIVE));    // ARJ Compressed File Archive
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".bz2"),   ED2KFT_ARCHIVE));    // Bzip Compressed File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".cab"),   ED2KFT_ARCHIVE));    // Cabinet File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".cbr"),   ED2KFT_ARCHIVE));    // Comic Book RAR Archive
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".cbt"),   ED2KFT_ARCHIVE));    // Comic Book Tarball
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".cbz"),   ED2KFT_ARCHIVE));    // Comic Book ZIP Archive
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".gz"),    ED2KFT_ARCHIVE));    // Gnu Zipped File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".hqx"),   ED2KFT_ARCHIVE));    // BinHex 4.0 Encoded File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".lha"),   ED2KFT_ARCHIVE));    // LHARC Compressed Archive
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".lzh"),   ED2KFT_ARCHIVE));    // LZH Compressed File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".msi"),   ED2KFT_ARCHIVE));    // Microsoft Installer File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".pak"),   ED2KFT_ARCHIVE));    // PAK (Packed) File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".par"),   ED2KFT_ARCHIVE));    // Parchive Index File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".par2"),  ED2KFT_ARCHIVE));    // Parchive 2 Index File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".rar"),   ED2KFT_ARCHIVE));    // WinRAR Compressed Archive
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".sea"),   ED2KFT_ARCHIVE));    // Self-Extracting Archive (Mac)
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".sit"),   ED2KFT_ARCHIVE));    // Stuffit Archive
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".sitx"),  ED2KFT_ARCHIVE));    // Stuffit X Archive
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".tar"),   ED2KFT_ARCHIVE));    // Consolidated Unix File Archive
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".tbz2"),  ED2KFT_ARCHIVE));    // Tar BZip 2 Compressed File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".tgz"),   ED2KFT_ARCHIVE));    // Gzipped Tar File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".uc2"),   ED2KFT_ARCHIVE));    // UltraCompressor 2 Archive
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".xpi"),   ED2KFT_ARCHIVE));    // Mozilla Installer Package
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".z"),     ED2KFT_ARCHIVE));    // Unix Compressed File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".zip"),   ED2KFT_ARCHIVE));    // Zipped File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".zoo"),   ED2KFT_ARCHIVE));    // Zoo Archive

            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".bat"),   ED2KFT_PROGRAM));    // Batch File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".cmd"),   ED2KFT_PROGRAM));    // Command File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".com"),   ED2KFT_PROGRAM));    // COM File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".exe"),   ED2KFT_PROGRAM));    // Executable File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".hta"),   ED2KFT_PROGRAM));    // HTML Application
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".js"),    ED2KFT_PROGRAM));    // Java Script
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".jse"),   ED2KFT_PROGRAM));    // Encoded  Java Script
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".msc"),   ED2KFT_PROGRAM));    // Microsoft Common Console File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".vbe"),   ED2KFT_PROGRAM));    // Encoded Visual Basic Script File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".vbs"),   ED2KFT_PROGRAM));    // Visual Basic Script File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".wsf"),   ED2KFT_PROGRAM));    // Windows Script File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".wsh"),   ED2KFT_PROGRAM));    // Windows Scripting Host File

            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".bin"),   ED2KFT_CDIMAGE));    // CD Image
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".bwa"),   ED2KFT_CDIMAGE));    // BlindWrite Disk Information File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".bwi"),   ED2KFT_CDIMAGE));    // BlindWrite CD/DVD Disc Image
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".bws"),   ED2KFT_CDIMAGE));    // BlindWrite Sub Code File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".bwt"),   ED2KFT_CDIMAGE));    // BlindWrite 4 Disk Image
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".ccd"),   ED2KFT_CDIMAGE));    // CloneCD Disk Image
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".cue"),   ED2KFT_CDIMAGE));    // Cue Sheet File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".dmg"),   ED2KFT_CDIMAGE));    // Mac OS X Disk Image
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".dmz"),   ED2KFT_CDIMAGE));
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".img"),   ED2KFT_CDIMAGE));    // Disk Image Data File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".iso"),   ED2KFT_CDIMAGE));    // Disc Image File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mdf"),   ED2KFT_CDIMAGE));    // Media Disc Image File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".mds"),   ED2KFT_CDIMAGE));    // Media Descriptor File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".nrg"),   ED2KFT_CDIMAGE));    // Nero CD/DVD Image File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".sub"),   ED2KFT_CDIMAGE));    // Subtitle File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".toast"), ED2KFT_CDIMAGE));    // Toast Disc Image

            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".chm"),   ED2KFT_DOCUMENT));   // Compiled HTML Help File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".css"),   ED2KFT_DOCUMENT));   // Cascading Style Sheet
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".diz"),   ED2KFT_DOCUMENT));   // Description in Zip File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".doc"),   ED2KFT_DOCUMENT));   // Document File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".dot"),   ED2KFT_DOCUMENT));   // Document Template File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".hlp"),   ED2KFT_DOCUMENT));   // Help File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".htm"),   ED2KFT_DOCUMENT));   // HTML File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".html"),  ED2KFT_DOCUMENT));   // HTML File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".nfo"),   ED2KFT_DOCUMENT));   // Warez Information File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".odp"),   ED2KFT_DOCUMENT));   // OpenDocument Presentation
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".ods"),   ED2KFT_DOCUMENT));   // OpenDocument Spreadsheet
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".odt"),   ED2KFT_DOCUMENT));   // OpenDocument File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".otp"),   ED2KFT_DOCUMENT));   // OpenDocument Presentation Template
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".ott"),   ED2KFT_DOCUMENT));   // OpenDocument Template File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".ots"),   ED2KFT_DOCUMENT));   // OpenDocument Spreadsheet Template
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".pdf"),   ED2KFT_DOCUMENT));   // Portable Document Format File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".pps"),   ED2KFT_DOCUMENT));   // PowerPoint Slide Show
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".ppt"),   ED2KFT_DOCUMENT));   // PowerPoint Presentation
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".ps"),    ED2KFT_DOCUMENT));   // PostScript File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".rtf"),   ED2KFT_DOCUMENT));   // Rich Text Format File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".stc"),   ED2KFT_DOCUMENT));   // OpenOffice.org 1.0 Spreadsheet Template
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".sti"),   ED2KFT_DOCUMENT));   // OpenOffice.org 1.0 Presentation Template
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".stw"),   ED2KFT_DOCUMENT));   // OpenOffice.org 1.0 Document Template File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".sxc"),   ED2KFT_DOCUMENT));   // OpenOffice.org 1.0 Spreadsheet
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".sxi"),   ED2KFT_DOCUMENT));   // OpenOffice.org 1.0 Presentation
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".sxw"),   ED2KFT_DOCUMENT));   // OpenOffice.org 1.0 Document File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".text"),  ED2KFT_DOCUMENT));   // General Text File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".txt"),   ED2KFT_DOCUMENT));   // Text File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".wri"),   ED2KFT_DOCUMENT));   // Windows Write Document
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".xls"),   ED2KFT_DOCUMENT));   // Microsoft Excel Spreadsheet
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".xlt"),   ED2KFT_DOCUMENT));   // Microsoft Excel Template
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".xml"),   ED2KFT_DOCUMENT));   // XML File
            ED2KFileTypesMap.insert(SED2KFileTypeMapElement(std::string(".emulecollection"), ED2KFT_EMULECOLLECTION));   // emulecollection
        }
    };

    CED2KFileTypes theED2KFileTypes;

    struct SED2KFileType
    {
        const char* pchExt;
        EED2KFileType iFileType;
    } g_aED2KFileTypes[] =
    {
        { (".aac"),   ED2KFT_AUDIO },     // Advanced Audio Coding File
        { (".ac3"),   ED2KFT_AUDIO },     // Audio Codec 3 File
        { (".aif"),   ED2KFT_AUDIO },     // Audio Interchange File Format
        { (".aifc"),  ED2KFT_AUDIO },     // Audio Interchange File Format
        { (".aiff"),  ED2KFT_AUDIO },     // Audio Interchange File Format
        { (".amr"),   ED2KFT_AUDIO },     // Adaptive Multi-Rate Codec File
        { (".ape"),   ED2KFT_AUDIO },     // Monkey's Audio Lossless Audio File
        { (".au"),    ED2KFT_AUDIO },     // Audio File (Sun, Unix)
        { (".aud"),   ED2KFT_AUDIO },     // General Audio File
        { (".audio"), ED2KFT_AUDIO },     // General Audio File
        { (".cda"),   ED2KFT_AUDIO },     // CD Audio Track
        { (".dmf"),   ED2KFT_AUDIO },     // Delusion Digital Music File
        { (".dsm"),   ED2KFT_AUDIO },     // Digital Sound Module
        { (".dts"),   ED2KFT_AUDIO },     // DTS Encoded Audio File
        { (".far"),   ED2KFT_AUDIO },     // Farandole Composer Module
        { (".flac"),  ED2KFT_AUDIO },     // Free Lossless Audio Codec File
        { (".it"),    ED2KFT_AUDIO },     // Impulse Tracker Module
        { (".m1a"),   ED2KFT_AUDIO },     // MPEG-1 Audio File
        { (".m2a"),   ED2KFT_AUDIO },     // MPEG-2 Audio File
        { (".m4a"),   ED2KFT_AUDIO },     // MPEG-4 Audio File
        { (".mdl"),   ED2KFT_AUDIO },     // DigiTrakker Module
        { (".med"),   ED2KFT_AUDIO },     // Amiga MED Sound File
        { (".mid"),   ED2KFT_AUDIO },     // MIDI File
        { (".midi"),  ED2KFT_AUDIO },     // MIDI File
        { (".mka"),   ED2KFT_AUDIO },     // Matroska Audio File
        { (".mod"),   ED2KFT_AUDIO },     // Amiga Music Module File
        { (".mp1"),   ED2KFT_AUDIO },     // MPEG-1 Audio File
        { (".mp2"),   ED2KFT_AUDIO },     // MPEG-2 Audio File
        { (".mp3"),   ED2KFT_AUDIO },     // MPEG-3 Audio File
        { (".mpa"),   ED2KFT_AUDIO },     // MPEG Audio File
        { (".mpc"),   ED2KFT_AUDIO },     // Musepack Compressed Audio File
        { (".mtm"),   ED2KFT_AUDIO },     // MultiTracker Module
        { (".ogg"),   ED2KFT_AUDIO },     // Ogg Vorbis Compressed Audio File
        { (".psm"),   ED2KFT_AUDIO },     // Protracker Studio Module
        { (".ptm"),   ED2KFT_AUDIO },     // PolyTracker Module
        { (".ra"),    ED2KFT_AUDIO },     // Real Audio File
        { (".rmi"),   ED2KFT_AUDIO },     // MIDI File
        { (".s3m"),   ED2KFT_AUDIO },     // Scream Tracker 3 Module
        { (".snd"),   ED2KFT_AUDIO },     // Audio File (Sun, Unix)
        { (".stm"),   ED2KFT_AUDIO },     // Scream Tracker 2 Module
        { (".umx"),   ED2KFT_AUDIO },     // Unreal Music Package
        { (".wav"),   ED2KFT_AUDIO },     // WAVE Audio File
        { (".wma"),   ED2KFT_AUDIO },     // Windows Media Audio File
        { (".xm"),    ED2KFT_AUDIO },     // Fasttracker 2 Extended Module

        { (".3g2"),   ED2KFT_VIDEO },     // 3GPP Multimedia File
        { (".3gp"),   ED2KFT_VIDEO },     // 3GPP Multimedia File
        { (".3gp2"),  ED2KFT_VIDEO },     // 3GPP Multimedia File
        { (".3gpp"),  ED2KFT_VIDEO },     // 3GPP Multimedia File
        { (".amv"),   ED2KFT_VIDEO },     // Anime Music Video File
        { (".asf"),   ED2KFT_VIDEO },     // Advanced Systems Format File
        { (".avi"),   ED2KFT_VIDEO },     // Audio Video Interleave File
        { (".bik"),   ED2KFT_VIDEO },     // BINK Video File
        { (".divx"),  ED2KFT_VIDEO },     // DivX-Encoded Movie File
        { (".dvr-ms"),ED2KFT_VIDEO },     // Microsoft Digital Video Recording
        { (".flc"),   ED2KFT_VIDEO },     // FLIC Video File
        { (".fli"),   ED2KFT_VIDEO },     // FLIC Video File
        { (".flic"),  ED2KFT_VIDEO },     // FLIC Video File
        { (".flv"),   ED2KFT_VIDEO },     // Flash Video File
        { (".hdmov"), ED2KFT_VIDEO },     // High-Definition QuickTime Movie
        { (".ifo"),   ED2KFT_VIDEO },     // DVD-Video Disc Information File
        { (".m1v"),   ED2KFT_VIDEO },     // MPEG-1 Video File
        { (".m2t"),   ED2KFT_VIDEO },     // MPEG-2 Video Transport Stream
        { (".m2ts"),  ED2KFT_VIDEO },     // MPEG-2 Video Transport Stream
        { (".m2v"),   ED2KFT_VIDEO },     // MPEG-2 Video File
        { (".m4b"),   ED2KFT_VIDEO },     // MPEG-4 Video File
        { (".m4v"),   ED2KFT_VIDEO },     // MPEG-4 Video File
        { (".mkv"),   ED2KFT_VIDEO },     // Matroska Video File
        { (".mov"),   ED2KFT_VIDEO },     // QuickTime Movie File
        { (".movie"), ED2KFT_VIDEO },     // QuickTime Movie File
        { (".mp1v"),  ED2KFT_VIDEO },     // MPEG-1 Video File
        { (".mp2v"),  ED2KFT_VIDEO },     // MPEG-2 Video File
        { (".mp4"),   ED2KFT_VIDEO },     // MPEG-4 Video File
        { (".mpe"),   ED2KFT_VIDEO },     // MPEG Video File
        { (".mpeg"),  ED2KFT_VIDEO },     // MPEG Video File
        { (".mpg"),   ED2KFT_VIDEO },     // MPEG Video File
        { (".mpv"),   ED2KFT_VIDEO },     // MPEG Video File
        { (".mpv1"),  ED2KFT_VIDEO },     // MPEG-1 Video File
        { (".mpv2"),  ED2KFT_VIDEO },     // MPEG-2 Video File
        { (".ogm"),   ED2KFT_VIDEO },     // Ogg Media File
        { (".pva"),   ED2KFT_VIDEO },     // MPEG Video File
        { (".qt"),    ED2KFT_VIDEO },     // QuickTime Movie
        { (".ram"),   ED2KFT_VIDEO },     // Real Audio Media
        { (".ratdvd"),ED2KFT_VIDEO },     // RatDVD Disk Image
        { (".rm"),    ED2KFT_VIDEO },     // Real Media File
        { (".rmm"),   ED2KFT_VIDEO },     // Real Media File
        { (".rmvb"),  ED2KFT_VIDEO },     // Real Video Variable Bit Rate File
        { (".rv"),    ED2KFT_VIDEO },     // Real Video File
        { (".smil"),  ED2KFT_VIDEO },     // SMIL Presentation File
        { (".smk"),   ED2KFT_VIDEO },     // Smacker Compressed Movie File
        { (".swf"),   ED2KFT_VIDEO },     // Macromedia Flash Movie
        { (".tp"),    ED2KFT_VIDEO },     // Video Transport Stream File
        { (".ts"),    ED2KFT_VIDEO },     // Video Transport Stream File
        { (".vid"),   ED2KFT_VIDEO },     // General Video File
        { (".video"), ED2KFT_VIDEO },     // General Video File
        { (".vob"),   ED2KFT_VIDEO },     // DVD Video Object File
        { (".vp6"),   ED2KFT_VIDEO },     // TrueMotion VP6 Video File
        { (".wm"),    ED2KFT_VIDEO },     // Windows Media Video File
        { (".wmv"),   ED2KFT_VIDEO },     // Windows Media Video File
        { (".xvid"),  ED2KFT_VIDEO },     // Xvid-Encoded Video File

        { (".bmp"),   ED2KFT_IMAGE },     // Bitmap Image File
        { (".emf"),   ED2KFT_IMAGE },     // Enhanced Windows Metafile
        { (".gif"),   ED2KFT_IMAGE },     // Graphical Interchange Format File
        { (".ico"),   ED2KFT_IMAGE },     // Icon File
        { (".jfif"),  ED2KFT_IMAGE },     // JPEG File Interchange Format
        { (".jpe"),   ED2KFT_IMAGE },     // JPEG Image File
        { (".jpeg"),  ED2KFT_IMAGE },     // JPEG Image File
        { (".jpg"),   ED2KFT_IMAGE },     // JPEG Image File
        { (".pct"),   ED2KFT_IMAGE },     // PICT Picture File
        { (".pcx"),   ED2KFT_IMAGE },     // Paintbrush Bitmap Image File
        { (".pic"),   ED2KFT_IMAGE },     // PICT Picture File
        { (".pict"),  ED2KFT_IMAGE },     // PICT Picture File
        { (".png"),   ED2KFT_IMAGE },     // Portable Network Graphic
        { (".psd"),   ED2KFT_IMAGE },     // Photoshop Document
        { (".psp"),   ED2KFT_IMAGE },     // Paint Shop Pro Image File
        { (".tga"),   ED2KFT_IMAGE },     // Targa Graphic
        { (".tif"),   ED2KFT_IMAGE },     // Tagged Image File
        { (".tiff"),  ED2KFT_IMAGE },     // Tagged Image File
        { (".wmf"),   ED2KFT_IMAGE },     // Windows Metafile
        { (".wmp"),   ED2KFT_IMAGE },     // Windows Media Photo File
        { (".xif"),   ED2KFT_IMAGE },     // ScanSoft Pagis Extended Image Format File

        { (".7z"),    ED2KFT_ARCHIVE },   // 7-Zip Compressed File
        { (".ace"),   ED2KFT_ARCHIVE },   // WinAce Compressed File
        { (".alz"),   ED2KFT_ARCHIVE },   // ALZip Archive
        { (".arc"),   ED2KFT_ARCHIVE },   // Compressed File Archive
        { (".arj"),   ED2KFT_ARCHIVE },   // ARJ Compressed File Archive
        { (".bz2"),   ED2KFT_ARCHIVE },   // Bzip Compressed File
        { (".cab"),   ED2KFT_ARCHIVE },   // Cabinet File
        { (".cbr"),   ED2KFT_ARCHIVE },   // Comic Book RAR Archive
        { (".cbz"),   ED2KFT_ARCHIVE },   // Comic Book ZIP Archive
        { (".gz"),    ED2KFT_ARCHIVE },   // Gnu Zipped File
        { (".hqx"),   ED2KFT_ARCHIVE },   // BinHex 4.0 Encoded File
        { (".lha"),   ED2KFT_ARCHIVE },   // LHARC Compressed Archive
        { (".lzh"),   ED2KFT_ARCHIVE },   // LZH Compressed File
        { (".msi"),   ED2KFT_ARCHIVE },   // Microsoft Installer File
        { (".pak"),   ED2KFT_ARCHIVE },   // PAK (Packed) File
        { (".par"),   ED2KFT_ARCHIVE },   // Parchive Index File
        { (".par2"),  ED2KFT_ARCHIVE },   // Parchive 2 Index File
        { (".rar"),   ED2KFT_ARCHIVE },   // WinRAR Compressed Archive
        { (".sit"),   ED2KFT_ARCHIVE },   // Stuffit Archive
        { (".sitx"),  ED2KFT_ARCHIVE },   // Stuffit X Archive
        { (".tar"),   ED2KFT_ARCHIVE },   // Consolidated Unix File Archive
        { (".tbz2"),  ED2KFT_ARCHIVE },   // Tar BZip 2 Compressed File
        { (".tgz"),   ED2KFT_ARCHIVE },   // Gzipped Tar File
        { (".xpi"),   ED2KFT_ARCHIVE },   // Mozilla Installer Package
        { (".z"),     ED2KFT_ARCHIVE },   // Unix Compressed File
        { (".zip"),   ED2KFT_ARCHIVE },   // Zipped File

        { (".bat"),   ED2KFT_PROGRAM },   // Batch File
        { (".cmd"),   ED2KFT_PROGRAM },   // Command File
        { (".com"),   ED2KFT_PROGRAM },   // COM File
        { (".exe"),   ED2KFT_PROGRAM },   // Executable File
        { (".hta"),   ED2KFT_PROGRAM },   // HTML Application
        { (".js"),    ED2KFT_PROGRAM },   // Java Script
        { (".jse"),   ED2KFT_PROGRAM },   // Encoded  Java Script
        { (".msc"),   ED2KFT_PROGRAM },   // Microsoft Common Console File
        { (".vbe"),   ED2KFT_PROGRAM },   // Encoded Visual Basic Script File
        { (".vbs"),   ED2KFT_PROGRAM },   // Visual Basic Script File
        { (".wsf"),   ED2KFT_PROGRAM },   // Windows Script File
        { (".wsh"),   ED2KFT_PROGRAM },   // Windows Scripting Host File

        { (".bin"),   ED2KFT_CDIMAGE },   // CD Image
        { (".bwa"),   ED2KFT_CDIMAGE },   // BlindWrite Disk Information File
        { (".bwi"),   ED2KFT_CDIMAGE },   // BlindWrite CD/DVD Disc Image
        { (".bws"),   ED2KFT_CDIMAGE },   // BlindWrite Sub Code File
        { (".bwt"),   ED2KFT_CDIMAGE },   // BlindWrite 4 Disk Image
        { (".ccd"),   ED2KFT_CDIMAGE },   // CloneCD Disk Image
        { (".cue"),   ED2KFT_CDIMAGE },   // Cue Sheet File
        { (".dmg"),   ED2KFT_CDIMAGE },   // Mac OS X Disk Image
        { (".img"),   ED2KFT_CDIMAGE },   // Disk Image Data File
        { (".iso"),   ED2KFT_CDIMAGE },   // Disc Image File
        { (".mdf"),   ED2KFT_CDIMAGE },   // Media Disc Image File
        { (".mds"),   ED2KFT_CDIMAGE },   // Media Descriptor File
        { (".nrg"),   ED2KFT_CDIMAGE },   // Nero CD/DVD Image File
        { (".sub"),   ED2KFT_CDIMAGE },   // Subtitle File
        { (".toast"), ED2KFT_CDIMAGE },   // Toast Disc Image

        { (".chm"),   ED2KFT_DOCUMENT },  // Compiled HTML Help File
        { (".css"),   ED2KFT_DOCUMENT },  // Cascading Style Sheet
        { (".diz"),   ED2KFT_DOCUMENT },  // Description in Zip File
        { (".doc"),   ED2KFT_DOCUMENT },  // Document File
        { (".dot"),   ED2KFT_DOCUMENT },  // Document Template File
        { (".hlp"),   ED2KFT_DOCUMENT },  // Help File
        { (".htm"),   ED2KFT_DOCUMENT },  // HTML File
        { (".html"),  ED2KFT_DOCUMENT },  // HTML File
        { (".nfo"),   ED2KFT_DOCUMENT },  // Warez Information File
        { (".pdf"),   ED2KFT_DOCUMENT },  // Portable Document Format File
        { (".pps"),   ED2KFT_DOCUMENT },  // PowerPoint Slide Show
        { (".ppt"),   ED2KFT_DOCUMENT },  // PowerPoint Presentation
        { (".ps"),    ED2KFT_DOCUMENT },  // PostScript File
        { (".rtf"),   ED2KFT_DOCUMENT },  // Rich Text Format File
        { (".text"),  ED2KFT_DOCUMENT },  // General Text File
        { (".txt"),   ED2KFT_DOCUMENT },  // Text File
        { (".wri"),   ED2KFT_DOCUMENT },  // Windows Write Document
        { (".xls"),   ED2KFT_DOCUMENT },  // Microsoft Excel Spreadsheet
        { (".xml"),   ED2KFT_DOCUMENT },  // XML File
        { (".emulecollection"), ED2KFT_EMULECOLLECTION }
    };


    EED2KFileType GetED2KFileTypeID(const std::string& strFileName)
    {
        std::string::size_type nPos = strFileName.find_last_of(".");

        if (nPos == std::string::npos)
        {
            return ED2KFT_ANY;
        }

        std::string strExt = strFileName.substr(nPos);

        // simple to lower because we can't parse national extensions in file
        std::transform(strExt.begin(), strExt.end(), strExt.begin(), boost::bind(std::tolower<char>, _1, std::locale(""))); //std::bind2nd(std::ptr_fun(std::tolower<char>), loc));
        SED2KFileTypeMap::iterator it = ED2KFileTypesMap.find(strExt);
        if (it != ED2KFileTypesMap.end())
        {
            return it->second;
        }

        return ED2KFT_ANY;
    }

    // Retuns the ed2k file type string ID which is to be used for publishing+searching
    std::string GetED2KFileTypeSearchTerm(EED2KFileType iFileID)
    {
        if (iFileID == ED2KFT_AUDIO)            return ED2KFTSTR_AUDIO;
        if (iFileID == ED2KFT_VIDEO)            return ED2KFTSTR_VIDEO;
        if (iFileID == ED2KFT_IMAGE)            return ED2KFTSTR_IMAGE;
        if (iFileID == ED2KFT_PROGRAM)          return ED2KFTSTR_PROGRAM;
        if (iFileID == ED2KFT_DOCUMENT)         return ED2KFTSTR_DOCUMENT;
        // NOTE: Archives and CD-Images are published+searched with file type "Pro"
        // NOTE: If this gets changed, the function 'GetED2KFileTypeSearchID' also needs to get updated!
        if (iFileID == ED2KFT_ARCHIVE)          return ED2KFTSTR_PROGRAM;
        if (iFileID == ED2KFT_CDIMAGE)          return ED2KFTSTR_PROGRAM;
        if (iFileID == ED2KFT_EMULECOLLECTION)  return ED2KFTSTR_EMULECOLLECTION;
        return std::string();
    }

    // Retuns the ed2k file type integer ID which is to be used for publishing+searching
    EED2KFileType GetED2KFileTypeSearchID(EED2KFileType iFileID)
    {
        if (iFileID == ED2KFT_AUDIO)            return ED2KFT_AUDIO;
        if (iFileID == ED2KFT_VIDEO)            return ED2KFT_VIDEO;
        if (iFileID == ED2KFT_IMAGE)            return ED2KFT_IMAGE;
        if (iFileID == ED2KFT_PROGRAM)          return ED2KFT_PROGRAM;
        if (iFileID == ED2KFT_DOCUMENT)         return ED2KFT_DOCUMENT;
        // NOTE: Archives and CD-Images are published+searched with file type "Pro"
        // NOTE: If this gets changed, the function 'GetED2KFileTypeSearchTerm' also needs to get updated!
        if (iFileID == ED2KFT_ARCHIVE)          return ED2KFT_PROGRAM;
        if (iFileID == ED2KFT_CDIMAGE)          return ED2KFT_PROGRAM;
        return ED2KFT_ANY;
    }

    // Returns a file type which is used eMule internally only, examining the extention of the given filename
    std::string GetFileTypeByName(const std::string& strFileName)
    {
        EED2KFileType iFileType = GetED2KFileTypeID(strFileName);

        switch (iFileType)
        {
            case ED2KFT_AUDIO:  return ED2KFTSTR_AUDIO;
            case ED2KFT_VIDEO:  return ED2KFTSTR_VIDEO;
            case ED2KFT_IMAGE:  return ED2KFTSTR_IMAGE;
            case ED2KFT_DOCUMENT:   return ED2KFTSTR_DOCUMENT;
            case ED2KFT_PROGRAM:    return ED2KFTSTR_PROGRAM;
            case ED2KFT_ARCHIVE:    return ED2KFTSTR_ARCHIVE;
            case ED2KFT_CDIMAGE:    return ED2KFTSTR_CDIMAGE;
            default:        return std::string();
        }
    }

    known_file_entry::known_file_entry()
    {

    }

    known_file_entry::known_file_entry(const md4_hash& hFile,
                                        const std::vector<md4_hash>& hSet,
                                        const fs::path& p,
                                        size_type nFilesize,
                                        boost::uint32_t nAccepted,
                                        boost::uint32_t nRequested,
                                        boost::uint64_t nTransferred,
                                        boost::uint8_t nPriority) :
                                        m_nLastChanged(0),
                                        m_hFile(hFile)
    {
        fs::path native_p = convert_to_native(p.string());
        m_nLastChanged = fs::last_write_time(native_p);
        boost::uint64_t nFileSize       = nFilesize;
        __file_size fs_trans;
        fs_trans.nQuadPart = nTransferred;

        m_hash_list.m_collection.assign(hSet.begin(), hSet.end());
        m_list.add_tag(make_string_tag(p.leaf(), FT_FILENAME, true));
        m_list.add_tag(make_string_tag(p.leaf(), FT_FILENAME, true));  // write same name for backward compatibility
        m_list.add_tag(make_typed_tag(nFileSize, FT_FILESIZE, true));
        m_list.add_tag(make_typed_tag(fs_trans.nLowPart, FT_ATTRANSFERRED, true));
        m_list.add_tag(make_typed_tag(fs_trans.nHighPart, FT_ATTRANSFERREDHI, true));
        m_list.add_tag(make_typed_tag(nRequested, FT_ATREQUESTED, true));
        m_list.add_tag(make_typed_tag(nAccepted, FT_ATACCEPTED, true));
        m_list.add_tag(make_typed_tag(nPriority, FT_ULPRIORITY, true));
    }

    void known_file_entry::dump() const
    {
        DBG("known_file_entry::dump(TS: " << m_nLastChanged
                << " " << m_hFile
                << " hash list size: " <<  m_hash_list.m_collection.size()
                << " tag list size: " << m_list.count());
    }

    known_file_collection::known_file_collection() : m_nHeader(MET_HEADER_WITH_LARGEFILES)
    {
    }

    void known_file_collection::dump() const
    {
        for (size_t n = 0; n < m_known_file_list.m_collection.size(); n++)
        {
            m_known_file_list.m_collection[n].dump();
        }
    }

    transfer_resume_data::transfer_resume_data(const md4_hash& hash, const fs::path& path, size_type size, const std::vector<char>& fr_data):
            m_hash(hash), m_filepath(path.string()), m_filesize(size)
    {
        if (!fr_data.empty())
        {
            m_fast_resume_data.add_tag(make_blob_tag(fr_data, FT_FAST_RESUME_DATA, true));
        }
    }

    transfer_resume_data::transfer_resume_data()
    {}

    file_hasher::file_hasher(add_transfer_handler handler, const std::string& known_filename) :
            m_bCancel(false),
            m_add_transfer(handler),
            m_known_filename(known_filename)
    {

    }

    void file_hasher::start()
    {
        m_thread.reset(new boost::thread(boost::ref(*this)));
    }

    void file_hasher::stop()
    {
        m_order.cancel();
        m_bCancel   = true;

        // when thread exists - wait it
        if (m_thread.get())
        {
            m_thread->join();
        }
    }

    void file_hasher::operator()()
    {
        try
        {            
            // prepare migration structure
            known_file_collection kfc;

            if (!m_known_filename.empty())
            {
                std::ifstream fstream(convert_to_native(m_known_filename).c_str(), std::ios_base::binary | std::ios_base::in);

                if (fstream)
                {
                    libed2k::archive::ed2k_iarchive ifa(fstream);

                    try
                    {
                        ifa >> kfc;
                    }
                    catch(libed2k_exception&)
                    {
                        kfc.m_known_file_list.m_collection.clear();
                    }
                }
            }

            while(1)
            {
                // we have UTF-8 strings in path pair pair
                std::pair<fs::path, fs::path> pp = m_order.popWait(); 

                DBG("{file_hasher: " << convert_to_native(bom_filter(pp.second.string())) << "}");

                try
                {
                    add_transfer_params atp;
                    // since transfers work in UTF-8 code page we save paths in this code page
                    atp.collection_path = pp.first;   // store collection path as is - in UTF-8
                    atp.file_path       = pp.second;  // store file path as is in UTF-8
                    atp.seed_mode       = true;       // it is seed!

                    // generate operations path in native code page
                    fs::path p = convert_to_native(bom_filter(pp.second.string()));

                    if (!fs::exists(p) || !fs::is_regular_file(p))
                    {
                        throw libed2k_exception(errors::file_unavaliable);
                    }

                    uintmax_t nFileSize = fs::file_size(p);

                    if (nFileSize == 0)
                    {
                        throw libed2k_exception(errors::filesize_is_zero);
                    }

                    atp.file_size = nFileSize;

                    // search file in migration container
                    for (size_t n = 0; n < kfc.m_known_file_list.m_collection.size(); n++)
                    {
                        // pass file when change time isn't equal
                        if ((fs::last_write_time(p)) != kfc.m_known_file_list.m_collection[n].m_nLastChanged)
                        {
                            continue;
                        }

                        // ok, we have equal change time - let's check filename
                        if (bom_filter(pp.second.filename()) != bom_filter(kfc.m_known_file_list.m_collection[n].m_list.getStringTagByNameId(FT_FILENAME)))
                        {
                            DBG("orig: " << convert_to_native(pp.second.filename()) 
                                << " isn't equal: " << convert_to_native(bom_filter(kfc.m_known_file_list.m_collection[n].m_list.getStringTagByNameId(FT_FILENAME))));
                            continue;
                        }

                        atp.file_hash = kfc.m_known_file_list.m_collection[n].m_hFile;

                        if (kfc.m_known_file_list.m_collection[n].m_hash_list.m_collection.empty())
                        {
                            // when file contain only one hash - we save main hash directly into container
                            atp.piece_hashses.push_back(kfc.m_known_file_list.m_collection[n].m_hFile);
                        }
                        else
                        {
                            atp.piece_hashses = kfc.m_known_file_list.m_collection[n].m_hash_list.m_collection;
                        }

                        for (size_t j = 0; j < kfc.m_known_file_list.m_collection[n].m_list.count(); j++)
                        {
                            const boost::shared_ptr<base_tag> p = kfc.m_known_file_list.m_collection[n].m_list[j];

                            switch(p->getNameId())
                            {
                                case FT_FILESIZE:
                                    //atp.file_size = p->asInt();
                                    break;
                                case FT_ATTRANSFERRED:
                                    atp.transferred += p->asInt();
                                    break;
                                case FT_ATTRANSFERREDHI:
                                    atp.transferred += (p->asInt() << 32);
                                    break;
                                case FT_ATREQUESTED:
                                    atp.requested = p->asInt();
                                    break;
                                case FT_ATACCEPTED:
                                    atp.accepted = p->asInt();
                                    break;
                                case FT_ULPRIORITY:
                                    atp.priority = p->asInt();
                                    break;
                                default:
                                    // ignore unused tags like
                                    // FT_PERMISSIONS
                                    // FT_AICH_HASH:
                                    // and all kad tags
                                    // also FT_FILENAME was already checked
                                    break;
                            }
                        }

                        DBG("metadata was migrated for {" << convert_to_native(atp.file_path.string()) << "}");
                        break;
                    }

                    // execute real hashing when migration got nothing
                    if (!atp.file_hash.defined())
                    {
                        namespace bio = boost::iostreams;
                        bool    bPartial = false; // check last part in file not full
                        bio::mapped_file_params mf_param;
                        mf_param.flags  = bio::mapped_file_base::readonly;
                        mf_param.path   = p.string();
                        mf_param.length = 0;


                        bio::mapped_file_source fsource;

                        uintmax_t nCurrentOffset = 0;
                        CryptoPP::Weak1::MD4 md4_hasher;

                        while(nCurrentOffset < nFileSize)
                        {
                            if (m_bCancel)
                            {
                                break;
                            }

                            uintmax_t nMapPosition = (nCurrentOffset / bio::mapped_file::alignment()) * bio::mapped_file::alignment();    // calculate appropriate mapping start position
                            uintmax_t nDataCorrection = nCurrentOffset - nMapPosition;                                          // offset to data start

                            // calculate map size
                            uintmax_t nMapLength = PIECE_SIZE*PIECE_COUNT_ALLOC;

                            // correct map length
                            if (nMapLength > (nFileSize - nCurrentOffset))
                            {
                                nMapLength = nFileSize - nCurrentOffset + nDataCorrection;
                            }
                            else
                            {
                                nMapLength += nDataCorrection;
                            }

                            mf_param.offset = nMapPosition;
                            mf_param.length = nMapLength;
                            fsource.open(mf_param);

                            size_type nLocalOffset = nDataCorrection; // start from data correction offset

                            DBG("{mpos: " << nMapPosition << "} {dt corr: "
                                    << nDataCorrection << "} {map len: "
                                    << nMapLength << "} {mpc: " << nMapLength / PIECE_SIZE << "}");

                            while(nLocalOffset < nMapLength)
                            {
                                if (m_bCancel)
                                {
                                    break;
                                }

                                // calculate current part size size
                                uintmax_t nLength = PIECE_SIZE;

                                if (PIECE_SIZE > nMapLength - nLocalOffset)
                                {
                                    nLength = nMapLength-nLocalOffset;
                                    bPartial = true;
                                }

                                libed2k::md4_hash hash;
                                md4_hasher.CalculateDigest(
                                    hash.getContainer(),
                                    reinterpret_cast<const unsigned char*>(fsource.data() + nLocalOffset), nLength);
                                atp.piece_hashses.push_back(hash);
                                // generate hash
                                nLocalOffset    += nLength;
                                nCurrentOffset  += nLength;
                            }

                            fsource.close();

                        }

                        if (m_bCancel)
                        {
                            break;
                        }

                        // when we don't have last partial piece - add special hash
                        if (!bPartial)
                        {
                            atp.piece_hashses.push_back(md4_hash::terminal);
                        }

                        if (atp.piece_hashses.size() > 1)
                        {
                            md4_hasher.CalculateDigest(
                                atp.file_hash.getContainer(),
                                reinterpret_cast<const unsigned char*>(&atp.piece_hashses[0]),
                                atp.piece_hashses.size()*libed2k::MD4_HASH_SIZE);
                        }
                        else
                        {
                            atp.file_hash = atp.piece_hashses[0];
                        }
                    }

                    m_add_transfer(atp);
                }
                catch(libed2k_exception& e)
                {
                    ERR("libed2k error on hashing: " << e.what());
                }
                catch(...) // hide all possible exceptions
                {
                    ERR("unhandled exception on hashing file: " << convert_to_native(bom_filter(pp.second.string())));
                }
            }
        }
        catch(libed2k_exception&)
        {
            // exit signal received
        }
    }

    void emule_binary_collection::dump() const
    {
        DBG("emule_collection::dump");
        DBG("version: " << m_nVersion);
        m_list.dump();
        m_files.dump();
    }

    bool emule_binary_collection::operator==(const emule_binary_collection& ec) const
    {
        if (m_nVersion != ec.m_nVersion || m_list != ec.m_list || m_files.m_size != ec.m_files.m_size)
        {
            return (false);
        }

        for (size_t n = 0; n < m_files.m_collection.size(); ++n)
        {
            if (m_files.m_collection[n] != ec.m_files.m_collection[n])
            {
                return (false);
            }
        }

        return (true);
    }

    // static
    emule_collection emule_collection::fromFile(const std::string& strFilename)
    {
        emule_collection ec;
        std::ifstream ifs(strFilename.c_str(), std::ios_base::in | std::ios_base::binary);

        if (ifs)
        {
            try
            {
                emule_binary_collection ebc;
                archive::ed2k_iarchive ifa(ifs);
                ifa >> ebc;

                for (size_t i = 0; i < ebc.m_files.m_collection.size(); ++i) // iterate each tag list
                {
                    std::string strFName;
                    boost::uint64_t nFilesize = 0;
                    md4_hash        hFile;

                    for (size_t j = 0; j < ebc.m_files.m_collection[i].count(); ++j)
                    {
                        const boost::shared_ptr<base_tag> p = ebc.m_files.m_collection[i][j];

                        switch(p->getNameId())
                        {
                            case FT_FILENAME:
                                strFName = p->asString();
                                break;
                            case FT_FILESIZE:
                                nFilesize = p->asInt();
                                break;
                            case FT_FILEHASH:
                                hFile = p->asHash();
                                break;
                            default:
                                //pass unused flags
                                break;
                        }

                    }

                    if (!strFName.empty() && hFile.defined())
                    {
                        ec.m_files.push_back(emule_collection_entry(strFName, nFilesize, hFile));
                    }
                }

            }
            catch(const libed2k_exception& )
            {
                // hide exception and go to text loading
            }

            // normalize stream after previous errors
            ifs.clear();
            ifs.seekg(0, std::ios_base::beg);
            std::string line;

            while (std::getline(ifs, line, (char)10 /* LF */))
            {
                int last = line.size()-1;

                if ((1 < last) && ((char)13 /* CR */ == line.at(last)))
                {
                    line.erase(last);
                }

                ec.add_link(line);
            }

        }

        return (ec);
    }

    // static
    std::string emule_collection::toLink(const std::string& strFilename, size_type nFilesize, const md4_hash& hFile)
    {
        std::stringstream retvalue;
        // ed2k://|file|fileName|fileSize|fileHash|/
        retvalue
        << "ed2k://|file|" << strFilename
        << "|" << nFilesize
        << "|" << hFile.toString()
        << "|/";

        return (retvalue.str());
    }

    emule_collection_entry emule_collection::fromLink(const std::string& strLink)
    {
        emule_collection_entry ecl;
        // 12345678901234       56       7 + 32 + 89 = 19+32=51
        // ed2k://|file|fileName|fileSize|fileHash|/
        if (strLink.size() < 51 || strLink.substr(0,13) != "ed2k://|file|" ||
            strLink.substr(strLink.size()-2) != "|/")
        {
            return ecl;
        }

        size_t iName = strLink.find("|",13);

        if (iName == std::string::npos)
        {
            return ecl;
        }

        std::string fileName = url_decode(strLink.substr(13,iName-13));

        size_t iSize = strLink.find("|",iName+1);

        if (iSize == std::string::npos)
        {
            return ecl;
        }

        std::stringstream sFileSize;
        sFileSize << strLink.substr(iName+1,iSize-iName-1);
        boost::uint64_t fileSize;

        if ((sFileSize >> std::dec >> fileSize).fail())
        {
            return ecl;
        }

        size_t iHash = strLink.find("|",iSize+1);
        if (iHash == std::string::npos)
        {
            return ecl;
        }

        std::string fileHash = strLink.substr(iSize+1,32);

        return emule_collection_entry(fileName, fileSize, md4_hash::fromString(fileHash));
    }

    emule_collection_entry pending2collectionentry(const pending_file& f)
    {
        if (!f.m_hash.defined())
        {
            // internal error
            throw libed2k_exception(errors::pending_file_entry_in_transform);
        }

        return emule_collection_entry(f.m_path.filename(), f.m_size, f.m_hash);
    }

    //static
    emule_collection emule_collection::fromPending(const pending_collection& pending)
    {
        emule_collection ecoll;
        std::transform(pending.m_files.begin(), pending.m_files.end(), std::back_inserter(ecoll.m_files), std::ptr_fun(&pending2collectionentry));
        return ecoll;
    }

    bool emule_collection::save(const std::string& strFilename, bool binary /*false*/)
    {
        // don't save empty collections
        if (m_files.empty())
        {
            DBG("emule_collection::save empty");
            return false;
        }

        if (binary)
        {
            std::ofstream fstr(strFilename.c_str(), std::ios_base::out | std::ios_base::binary);

            if (fstr)
            {
                emule_binary_collection ebc;
                ebc.m_files.m_collection.resize(m_files.size());

                for (size_t n = 0; n < m_files.size(); ++n)
                {
                    boost::uint64_t filesize = m_files[n].m_filesize;
                    ebc.m_files.m_collection[n].add_tag(make_string_tag(m_files[n].m_filename, FT_FILENAME, true));
                    ebc.m_files.m_collection[n].add_tag(make_typed_tag(filesize, FT_FILESIZE, true));
                    ebc.m_files.m_collection[n].add_tag(make_typed_tag(m_files[n].m_filehash, FT_FILEHASH, true));
                }

                archive::ed2k_oarchive foa(fstr);
                foa << ebc;
                return true;
            }
        }
        else
        {
            std::ofstream fstr(strFilename.c_str(), std::ios_base::out);

            if (fstr)
            {
                for (size_t n = 0; n < m_files.size(); ++n)
                {
                    if (n != 0)
                    {
                        fstr << "\n";
                    }

                    fstr << get_ed2k_link(n);
                }

                return true;
            }
        }

        return (false);
    }

    bool emule_collection::add_link(const std::string& strLink)
    {
        emule_collection_entry ecl = fromLink(strLink);

        if (ecl.defined())
        {
            m_files.push_back(ecl);
            return (true);
        }

        return (false);
    }

    bool emule_collection::add_file(const std::string& strFilename, size_type nFilesize, const std::string& strFilehash)
    {
        md4_hash hash;

        if (strFilename.empty() || nFilesize == 0 || nFilesize > 0xffffffffLL || strFilehash.size() != md4_hash::hash_size*2)
        {
            return false;
        }

        hash = md4_hash::fromString(strFilehash);

        if (!hash.defined())
        {
            return false;
        }

        m_files.push_back(emule_collection_entry(strFilename, nFilesize, hash));

        return true;
    }

    const std::string emule_collection::get_ed2k_link(size_t nIndex)
    {
        if (nIndex < m_files.size())
        {
            return emule_collection::toLink(m_files.at(nIndex).m_filename, m_files.at(nIndex).m_filesize, m_files.at(nIndex).m_filehash);
        }

        return std::string("");
    }

    bool emule_collection::operator==(const std::deque<pending_file>& files) const
    {
        if (files.size() != m_files.size())
        {
            return false;
        }

        std::deque<md4_hash> hashes;
        hashes.resize(files.size());

        std::transform(files.begin(), files.end(), hashes.begin(), std::mem_fun_ref(&pending_file::get_hash));

        for (std::deque<emule_collection_entry>::const_iterator itr = m_files.begin();
                itr != m_files.end(); ++itr)
        {
            if (std::find(hashes.begin(), hashes.end(), itr->m_filehash) == hashes.end())
            {
                return false;
            }
        }

        return (true);
    }

    bool emule_collection::operator!=(const std::deque<pending_file>& files) const
    {
        return (!(*this == files));
    }

    bool emule_collection::operator==(const emule_collection& ecoll) const
    {
        if (m_files.size() != ecoll.m_files.size())
        {
            return (false);
        }

        return std::equal(m_files.begin(), m_files.end(), ecoll.m_files.begin());
    }
}
