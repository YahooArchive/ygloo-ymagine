package com.yahoo.test.ymagine;

import com.yahoo.ymagine.Ymagine;
import com.yahoo.ymagine.Vbitmap;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.OutputStream;
import java.io.IOException;

public class Main {
    private static final boolean DEBUG_PERF = true;

    public static void usage() {
        System.out.println("Ymagine version " + Ymagine.getVersion());
        System.out.flush();
    }

    public static void main(String[] args) {
        String filename = null;
        Vbitmap vbitmap;
        boolean inmemory = true;

        vbitmap = Vbitmap.create();

        if (args == null || args.length <= 0) {
            usage();
            return;
        }

        if (args[0].equals("quantize") || args[0].equals("decode") || args[0].equals("transcode")) {
            int maxWidth = 1024;
            int maxHeight = 1024;
            boolean decodeOnly = false;
            boolean transcodeOnly = false;
            long start, end;
            long elapsed = 0;

            if (args[0].equals("decode")) {
                decodeOnly = true;
            }
            if (args[0].equals("transcode")) {
                transcodeOnly = true;
            }
            
            for (int i = 1; i < args.length; i++) {
                filename = args[i];
                if (filename != null) {
                    if (decodeOnly) {
                        int rc = vbitmap.decode(filename, maxWidth, maxHeight);                    
                        System.out.println("Decode " + filename + " rc=" + rc +
                                           " size=" + vbitmap.getWidth() + "x" + vbitmap.getHeight());
                    } else if (transcodeOnly) {
                        FileInputStream is = null;
                        ByteArrayOutputStream memoryOut = null;
                        FileOutputStream fileOut = null;
                        OutputStream os;
                        int rc = -1;

                        try {
                            File ifile = new File(filename);
                            is = new FileInputStream(ifile);

                            if (inmemory) {
                                memoryOut = new ByteArrayOutputStream();
                                os = (OutputStream) memoryOut;
                            } else {
                                File ofile = new File(".", "thumb_" + ifile.getName());
                                fileOut = new FileOutputStream(ofile);
                                os = (OutputStream) fileOut;
                            }

                            if (DEBUG_PERF) {
                                start = System.nanoTime();
                            }

                            rc = Ymagine.transcode(is, os, maxWidth, maxHeight);

                            if (DEBUG_PERF) {
                                end = System.nanoTime();
                                elapsed = end - start;
                            }
                        } catch (Exception e) {
                        } finally {
                            if (is != null) {
                                try {
                                    is.close();
                                } catch (IOException e) {
                                    // do nothing
                                }
                            }
                            if (memoryOut != null) {
                                // System.out.println(String.format("Transcoded input in %d bytes", memoryOut.size()));
                                try {
                                    memoryOut.close();
                                } catch (IOException e) {
                                    // do nothing
                                }
                            }
                            if (fileOut != null) {
                                try {
                                    fileOut.close();
                                } catch (IOException e) {
                                    // do nothing
                                }
                            }
                        }

                        if (elapsed > 0) {
                            System.out.println("Transcoded image in " + elapsed / 1000 + " us");
                        }
                    } else {
                        int colors[] = Ymagine.quantize(filename, 8, 64, 64);
                        if (colors != null) {
                            int ncolors = colors.length / 2;
                            System.out.println("Found " + ncolors + " colors in " + filename);
                            for (int colorid = 0; colorid < ncolors; colorid++) {
                                int rgb = colors[2 * colorid];
                                int score = colors[2 * colorid + 1];
                                int hsv = Ymagine.RGBtoHSV(rgb);
                                int hue = (Ymagine.getHue(hsv) * 360) / 256;
                                System.out.println(String.format("  RGB=#%08X Hue=%3d score=%d", 
                                                                 rgb, hue, score));
                            }
                        }
                    }
                    System.out.flush();
                }
            }

            return;
        }

        /* Unsupported run option */
        usage();
        return;
    }
}
