/* Project Oberon RAM disk: the file-system image built by tools/mkpodisk.py,
 * placed in .data so the kernel can read and write it in place
 * (see po_devices.cpp). PO_DISK_IMAGE is the image path, passed by CMake.
 */
    .section .data
    .balign 16
    .global po_disk_image
po_disk_image:
    .incbin PO_DISK_IMAGE
    .global po_disk_image_end
po_disk_image_end:

    .section .note.GNU-stack,"",@progbits
