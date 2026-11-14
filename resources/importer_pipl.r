// OpenMedia Premiere Pro Importer macOS Rez PiPL Resource Definition
// Defines 'PiPL' resource with 'IMPT' kind, 'OMIP' (0x4F4D4950), and xImportEntry.

#define kOpenMediaImporterFourCC 0x4F4D4950
#define kOpenMediaImporterName   "OpenMedia Importer"

resource 'PiPL' (16000, kOpenMediaImporterName, purgeable) {
    {
        Kind {
            'IMPT'
        },
        Name {
            kOpenMediaImporterName
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
            kOpenMediaImporterName
        },
        ImporterFourCC {
            'OMIP' /* 0x4F4D4950 */
        }
    }
};
