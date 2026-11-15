// OpenMedia Premiere Pro High-Priority Importer macOS Rez PiPL Resource Definition
// PiPL name includes a trailing space ("OpenMedia Importer ") to prevent Adobe registration collision.

#define kOpenMediaImporterHPFourCC 0x4F4D4948
#define kOpenMediaImporterHPName   "OpenMedia Importer "

resource 'PiPL' (16000, kOpenMediaImporterHPName, purgeable) {
    {
        Kind {
            'IMPT'
        },
        Name {
            kOpenMediaImporterHPName
        },
        Version {
            (1 << 16) | 0
        },
        CodeMacIntel64 {
            "xImportEntry"
        },
        CodeMacARM64 {
            "xImportEntry"
        },
        ImporterMatchName {
            kOpenMediaImporterHPName
        },
        ImporterFourCC {
            'OMIH' /* 0x4F4D4948 */
        }
    }
};
