/**
 * Copyright 2013 Yahoo! Inc.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may
 * obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License. See accompanying LICENSE file.
 */

package com.yahoo.ymagine;

import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;

public class ByteBufferInputStream extends InputStream {
    private ByteBuffer mByteBuffer;

    public ByteBufferInputStream(ByteBuffer byteBuffer) {
        mByteBuffer = byteBuffer;
    }

    public int read() throws IOException {
        if (!mByteBuffer.hasRemaining()) {
            return -1;
        }
        return mByteBuffer.get();
    }

    public int read(byte[] buffer, int offset, int length) throws IOException {
        int count = Math.min(mByteBuffer.remaining(), length);
        if (count <= 0) {
            return -1;
        }

        mByteBuffer.get(buffer, offset, length);
        return count;
    }

    public int read(byte[] buffer) throws IOException {
        return read(buffer, 0, buffer.length);
    }

    public int available() throws IOException {
        return mByteBuffer.remaining();
    }
}
