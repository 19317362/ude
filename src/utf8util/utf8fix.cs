using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Linq;
using System.Diagnostics;
using System.IO;
using Ude;
//https://unicodebook.readthedocs.io/unicode_encodings.html

namespace utf8util
{
    public enum EncodingIndex
    {
        EI_GBK = 0,
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
        {//分片处理
            Ude.CharsetDetector cdet = new Ude.CharsetDetector();
            var ecs = new Encoding[(int)EncodingIndex.EI_MAX];
            int[] maxLen = new int[(int)EncodingIndex.EI_MAX] { 2, 2, 2, 3, 1 };//最大长度。 中文最大3个字节

            ecs[(int)EncodingIndex.EI_GBK] = Encoding.GetEncoding("GBK");
            ecs[(int)EncodingIndex.EI_UCS2_LE] = Encoding.Unicode;
            ecs[(int)EncodingIndex.EI_UCS2_BE] = Encoding.BigEndianUnicode;

            ecs[(int)EncodingIndex.EI_UTF8] = Encoding.BigEndianUnicode;
            ecs[(int)EncodingIndex.EI_ASCII] = Encoding.ASCII;

            EncodingIndex theIdx;

            var sb = new StringBuilder();
            int usedLen = 0;
            string theSlice;
            string prevCS = "";
            float prevPricse = 0;
            var theCache = new List<string>();
            int i;
            for ( i=0; i < buf.Length; ++i)
            {

                cdet.Feed(buf,i,1);
                Debug.WriteLine($"IsDone {cdet.IsDone()} {cdet.Charset} {cdet.Confidence}");
                if(! string.IsNullOrEmpty(cdet.Charset))
                {
                    if(prevCS != cdet.Charset)
                    {
                        if(string.IsNullOrEmpty(prevCS))
                        {
                            prevCS = cdet.Charset;
                        }
                        else
                        {//发生了切换
                            switch(prevCS)
                            {
                                case Charsets.UTF16_LE:
                                    theIdx = EncodingIndex.EI_UCS2_LE;
                                    break;
                                case Charsets.UTF16_BE:
                                    theIdx = EncodingIndex.EI_UCS2_BE;
                                    break;

                                case Charsets.UTF8:
                                    theIdx = EncodingIndex.EI_UTF8;
                                    break;
                                case Charsets.GB18030:
                                    theIdx = EncodingIndex.EI_GBK;
                                    break;
                                case Charsets.ASCII:
                                    theIdx = EncodingIndex.EI_ASCII;
                                    break;
                                default:
                                    throw new NotImplementedException();
                            }
                            --i;//回退最后一个
                            cdet.DataEnd();
                            cdet.Reset();
                            theSlice = ecs[(int)theIdx].GetString(buf,usedLen,i-usedLen);
                            usedLen = i;//记录下来已用位置
                            theCache.Add(theSlice);//增加进去
                            sb.Append(theSlice);
                        }
                    }
                    else
                    {

                    }
                }

            }
            cdet.DataEnd();

            if ( i>usedLen)//处理最后一批
            {
                switch (cdet.Charset)
                {
                    case Charsets.UTF16_LE:
                        theIdx = EncodingIndex.EI_UCS2_LE;
                        break;
                    case Charsets.UTF16_BE:
                        theIdx = EncodingIndex.EI_UCS2_BE;
                        break;

                    case Charsets.UTF8:
                        theIdx = EncodingIndex.EI_UTF8;
                        break;
                    case Charsets.GB18030:
                        theIdx = EncodingIndex.EI_GBK;
                        break;
                    case Charsets.ASCII:
                        theIdx = EncodingIndex.EI_ASCII;
                        break;
                    default:
                        throw new NotImplementedException();
                }
                theSlice = ecs[(int)theIdx].GetString(buf, usedLen, i - usedLen);
                theCache.Add(theSlice);//增加进去
                sb.Append(theSlice);
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
