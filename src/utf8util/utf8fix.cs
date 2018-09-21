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
    public enum InputState
    {

        NA,
        ASCII,
        MBCS
    };

    public static class ByteExt
    {
        public static bool IsASCII(this byte value)
        {
            // ASCII encoding replaces non-ascii with question marks, so we use UTF8 to see if multi-byte sequences are there
            return (value & 0x80) == 0;
        }
    }


    public class utf8fix
    {

        /// <summary>
        /// Determines whether the argument string can be represented with the ASCII (<see cref="Encoding.ASCII"/>) encoding.
        /// </summary>
        /// <param name="value">The value to check.</param>
        /// <returns>
        /// <c>true</c> if the specified value is ASCII; otherwise, <c>false</c>.
        /// </returns>
        //public static bool IsASCII(this string value)
        //{
        //    // ASCII encoding replaces non-ascii with question marks, so we use UTF8 to see if multi-byte sequences are there
        //    return Encoding.UTF8.GetByteCount(value) == value.Length;
        //}

        /// <summary>
        /// 把输入buf中的 fix成 UTF8 串
        /// </summary>
        /// <param name="buf"></param>
        /// <returns></returns>
        public string FixBuffer(byte[] buf)
        {//分片处理
            var ecs = new Encoding[(int)EncodingIndex.EI_MAX]{

                Encoding.GetEncoding("GBK"),
                Encoding.Unicode,
                Encoding.BigEndianUnicode,
                Encoding.UTF8,
                Encoding.ASCII
                };
            int[] maxLen = new int[(int)EncodingIndex.EI_MAX] { 2, 2, 2, 3, 1 };//最大长度。 中文最大3个字节

            var cspMBCS = new Ude.Core.MBCSGroupProber();


            EncodingIndex theIdx = EncodingIndex.EI_ASCII;

            var sb = new StringBuilder();
            int usedLen = 0;
            string theSlice;
            var theSlices = new List<string>();//记录下每个分片
            int i;
            InputState inputState = InputState.NA;

            EncodingIndex bomIdx = EncodingIndex.EI_GBK;
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
            //按状态机来做
            InputState newState = InputState.NA;
            Ude.Core.ProbingState curProbSt = Ude.Core.ProbingState.Detecting;
            for (; i < buf.Length; )
            {
                int remain = buf.Length - i;
                int validLen =0;
                var valid = IsValidUtf8(buf,i,remain,ref validLen);

                Debug.WriteLine($"UTF8 {i} {buf[i]:X2} valid:{valid} remain:{remain} new:{validLen}");
                if(valid)
                {
                    i+=validLen;
                }
                else
                {
                    
                    ++i;
                }

                // other than 0xa0, if every other character is ascii, the page is ascii
      
                

            }

//             if (i > usedLen)//处理最后一批
//             {
//                 theSlice = ecs[(int)theIdx].GetString(buf, usedLen, i - usedLen);
//                 theSlices.Add(theSlice);//增加进去
//                 sb.Append(theSlice);
//             }
            return sb.ToString();
        }


        /// <summary>
        /// 
        /// </summary>
        /// <param name="buffer"></param>
        /// <param name="position"></param>
        /// <param name="length"></param>
        /// <param name="bytes"></param>
        /// <returns></returns>
        public static bool IsValidUtf8(byte[] buffer, int position, int length, ref int bytes)
        {
            if (position + length > buffer.Length)
            {
                throw new ArgumentException("Invalid length");
            }

            if (length < 1)
            {
                bytes = 0;
                return false;
            }

            byte ch = buffer[position];

            if (ch <= 0x7F)
            {
                bytes = 1;
                return true;
            }

            if (ch >= 0xc2 && ch <= 0xdf)
            {
                if (length<2)
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
                if (length < 3)
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
                if ( length < 3)
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
                if (length < 4)
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
                if ( length < 4)
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
                if ( length < 4)
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


        /// <summary>
        /// https://en.wikipedia.org/wiki/GBK_(character_encoding)
        /// 判断 GBK
        /// </summary>
        /// <param name="buffer"></param>
        /// <param name="position"></param>
        /// <param name="length"></param>
        /// <param name="bytes"></param>
        /// <returns></returns>
        public static bool IsValidGbk(byte[] buffer, int position, int length, ref int bytes)
        {
            bool valid = false;
            bytes = 0;

            if (position + length > buffer.Length || length<1)
            {
                
            }
            else
            {
                if (length == 1)
                {
                    valid = (buffer[0] & 0x80) == 0;
                    if (valid)
                    {
                        bytes = 1;
                    }
                }
                else // length >=2
                {
                    byte ch0 = buffer[position];
                    byte ch1 = buffer[position + 1];

                    if (((ch0 >= 0xA1 && ch0 <= 0xA9) && (ch1 >= 0xA1 && ch1 <= 0xFE)) // BGK/1
                        || ((ch0 >= 0xB0 && ch0 <= 0xF7) && (ch1 >= 0xA1 && ch1 <= 0xFE)) // BGK/2
                        || ((ch0 >= 0x81 && ch0 <= 0xA0) && (ch1 >= 0x40 && ch1 <= 0xFE && ch1 != 0x7F)) // BGK/3
                        || ((ch0 >= 0xAA && ch0 <= 0xFE) && (ch1 >= 0x40 && ch1 <= 0xA0 && ch1 != 0x7F)) // BGK/4
                        || ((ch0 >= 0xA8 && ch0 <= 0xA9) && (ch1 >= 0x40 && ch1 <= 0xA0 && ch1 != 0x7F)) // BGK/5
                                                                                                         //|| ((ch0 >= 0xAA && ch0 <= 0xAF) && (ch1 >= 0xA1 && ch1 <= 0xF && ch1 != 0x7F)) // BGK/4
                        )
                    {
                        valid = true;
                        bytes = 2;

                    }
                }

            }


            return valid;
        }

    }
}
