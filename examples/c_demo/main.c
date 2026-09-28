#include "dicom_viewer/operations_c.h"

#include <stdio.h>

static int report_error(const char *operation, const dicom_viewer_document *document) {
    fprintf(stderr, "%s failed: %s\n", operation,
            dicom_viewer_document_last_error(document));
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <input-dicom-file> [output-dicom-file]\n", argv[0]);
        return 2;
    }

    dicom_viewer_document *document = NULL;
    if (dicom_viewer_document_create(&document) != 0) {
        fprintf(stderr, "Could not create a DICOM document\n");
        return 1;
    }

    const int status = dicom_viewer_document_load(document, argv[1]);
    if (status != 0) {
        const int result = report_error("load", document);
        dicom_viewer_document_destroy(document);
        return result;
    }

    printf("Loaded DICOM file: %s\n", argv[1]);

    if (argc == 3) {
        if (dicom_viewer_document_save_as(document, argv[2]) != 0) {
            const int result = report_error("save_as", document);
            dicom_viewer_document_destroy(document);
            return result;
        }
        printf("Saved a copy with save_as: %s\n", argv[2]);

        if (dicom_viewer_document_save(document) != 0) {
            const int result = report_error("save", document);
            dicom_viewer_document_destroy(document);
            return result;
        }
        printf("Saved again with save: %s\n", argv[2]);
    }

    dicom_viewer_document_destroy(document);
    return 0;
}
