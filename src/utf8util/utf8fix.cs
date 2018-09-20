using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Linq;
using System.Diagnostics;
using System.IO;
//https://unicodebook.readthedocs.io/unicode_encodings.html

namespace utf8util
{
    public enum EncodingIndex
    {
        EI_GBK=0,
        EI_UCS2_LE,
        EI_UCS2_BE,
        EI_UTF8,
        EI_ASCII,
        EI_MAX
    }
    public class utf8fix
    {
        /// <summary>
        /// 把输入buf中的 fix成 UTF8 串
        /// </summary>
        /// <param name="buf"></param>
        /// <returns></returns>
        public string FixBuffer(byte[] buf)
        {
            var ecs = new Encoding[(int)EncodingIndex.EI_MAX];
            int[] maxLen = new int[(int)EncodingIndex.EI_MAX] {2,2,2,3,1};//最大长度。 中文最大3个字节

            ecs[(int)EncodingIndex.EI_GBK] = Encoding.GetEncoding("GBK");
            ecs[(int)EncodingIndex.EI_UCS2_LE] = Encoding.Unicode;
            ecs[(int)EncodingIndex.EI_UCS2_BE] = Encoding.BigEndianUnicode;

            ecs[(int)EncodingIndex.EI_UTF8] = Encoding.BigEndianUnicode;
            ecs[(int)EncodingIndex.EI_ASCII] = Encoding.ASCII;

            EncodingIndex bomIdx = EncodingIndex.EI_GBK;
            int i;
            i = (buf.Length >= 3 && buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF) ? 3 : 0;
            if (i == 0)
            {
                if (buf.Length >= 2)
                {
                    if (buf[0] == 0xFF && buf[1] == 0xFE)//UCS2-LE
                    {//应该主要是这种方式
                        bomIdx = EncodingIndex.EI_UCS2_LE;
                        i = 2;
                    }
                    else if (buf[0] == 0xFE && buf[1] == 0xFF)//UCS2-BE
                    {
                        bomIdx = EncodingIndex.EI_UCS2_BE;
                        i = 2;
                    }

                }

            }
            else
            {//已经是UTF8-BOM
                bomIdx = EncodingIndex.EI_UTF8;

            }
            int j;
            int remainLen =0;
            int defLen = maxLen[(int)bomIdx];
            string strChar;
            var sb = new StringBuilder();
            int usedLen =0;
            for (; i < buf.Length; i+=usedLen)
            {
                remainLen = buf.Length - i;
                if(remainLen >= defLen)
                {
                    try
                    {
                        
                        strChar = ecs[(int)bomIdx].GetString(buf, i, defLen);
                        sb.Append(strChar);
                        usedLen = ecs[(int)bomIdx].GetByteCount(strChar);
                        continue;//解析OK
                    }
                    catch(DecoderFallbackException ex)
                    {
                        Debug.WriteLine(ex);
                        strChar ="";
                    }
                }
                for (j = 0; j < ecs.Length; ++j)
                {
                    if(j != (int)bomIdx) //不等于的才处理
                    {
                        if(remainLen >= maxLen[j])
                        {//够长
                            try
                            {
                                strChar = ecs[(int)bomIdx].GetString(buf, i, defLen);
                                sb.Append(strChar);
                                usedLen = ecs[(int)bomIdx].GetByteCount(strChar);
                                break;//解析OK

                            }
                            catch (DecoderFallbackException ex)
                            {
                                Debug.WriteLine(ex);
                                strChar = "";
                            }
                        }
                    }
                }
                if(usedLen >0)
                {//
                    continue;
                }
                else
                {
                    usedLen =1;//跳过一个
                    sb.Append($"0x{buf[i]:X2}");
                }
            }
            return sb.ToString();
        }



        public bool Check(string fileName)
        {
            using (BufferedStream fstream = new BufferedStream(File.OpenRead(fileName)))
            {
                return this.IsUtf8(fstream);
            }
        }

        /// <summary>
        /// Check if stream is utf8 encoded.
        /// Notice: stream is read completely in memory!
        /// </summary>
        /// <param name="stream">Stream to read from.</param>
        /// <returns>True if the whole stream is utf8 encoded.</returns>
        public bool IsUtf8(Stream stream)
        {
            int count = 4 * 1024;
            byte[] buffer;
            int read;
            while (true)
            {
                buffer = new byte[count];
                stream.Seek(0, SeekOrigin.Begin);
                read = stream.Read(buffer, 0, count);
                if (read < count)
                {
                    break;
                }
                buffer = null;
                count *= 2;
            }
            return IsUtf8(buffer, read);
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="buffer"></param>
        /// <param name="length"></param>
        /// <returns></returns>
        public static bool IsUtf8(byte[] buffer, int length)
        {
            int position = 0;
            int bytes = 0;
            while (position < length)
            {
                if (!IsValid(buffer, position, length, ref bytes))
                {
                    return false;
                }
                position += bytes;
            }
            return true;
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="buffer"></param>
        /// <param name="position"></param>
        /// <param name="length"></param>
        /// <param name="bytes"></param>
        /// <returns></returns>
        public static bool IsValid(byte[] buffer, int position, int length, ref int bytes)
        {
            if (length > buffer.Length)
            {
                throw new ArgumentException("Invalid length");
            }

            if (position > length - 1)
            {
                bytes = 0;
                return true;
            }

            byte ch = buffer[position];

            if (ch <= 0x7F)
            {
                bytes = 1;
                return true;
            }

            if (ch >= 0xc2 && ch <= 0xdf)
            {
                if (position >= length - 2)
                {
                    bytes = 0;
                    return false;
                }
                if (buffer[position + 1] < 0x80 || buffer[position + 1] > 0xbf)
                {
                    bytes = 0;
                    return false;
                }
                bytes = 2;
                return true;
            }

            if (ch == 0xe0)
            {
                if (position >= length - 3)
                {
                    bytes = 0;
                    return false;
                }

                if (buffer[position + 1] < 0xa0 || buffer[position + 1] > 0xbf ||
                    buffer[position + 2] < 0x80 || buffer[position + 2] > 0xbf)
                {
                    bytes = 0;
                    return false;
                }
                bytes = 3;
                return true;
            }


            if (ch >= 0xe1 && ch <= 0xef)
            {
                if (position >= length - 3)
                {
                    bytes = 0;
                    return false;
                }

                if (buffer[position + 1] < 0x80 || buffer[position + 1] > 0xbf ||
                    buffer[position + 2] < 0x80 || buffer[position + 2] > 0xbf)
                {
                    bytes = 0;
                    return false;
                }

                bytes = 3;
                return true;
            }

            if (ch == 0xf0)
            {
                if (position >= length - 4)
                {
                    bytes = 0;
                    return false;
                }

                if (buffer[position + 1] < 0x90 || buffer[position + 1] > 0xbf ||
                    buffer[position + 2] < 0x80 || buffer[position + 2] > 0xbf ||
                    buffer[position + 3] < 0x80 || buffer[position + 3] > 0xbf)
                {
                    bytes = 0;
                    return false;
                }

                bytes = 4;
                return true;
            }

            if (ch == 0xf4)
            {
                if (position >= length - 4)
                {
                    bytes = 0;
                    return false;
                }

                if (buffer[position + 1] < 0x80 || buffer[position + 1] > 0x8f ||
                    buffer[position + 2] < 0x80 || buffer[position + 2] > 0xbf ||
                    buffer[position + 3] < 0x80 || buffer[position + 3] > 0xbf)
                {
                    bytes = 0;
                    return false;
                }

                bytes = 4;
                return true;
            }

            if (ch >= 0xf1 && ch <= 0xf3)
            {
                if (position >= length - 4)
                {
                    bytes = 0;
                    return false;
                }

                if (buffer[position + 1] < 0x80 || buffer[position + 1] > 0xbf ||
                    buffer[position + 2] < 0x80 || buffer[position + 2] > 0xbf ||
                    buffer[position + 3] < 0x80 || buffer[position + 3] > 0xbf)
                {
                    bytes = 0;
                    return false;
                }

                bytes = 4;
                return true;
            }

            return false;
        }

    }
}
