using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
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
        public bool HasInvalidChar(byte[] buf)
        {
            //ERR    EF BF BD
            //BOM:EF BB BF                                        

            int i;
            bool ret = false;
            for(i=0;i<buf.Length -3;++i)
            {
                if(buf[i]== 0xEF
                    &&buf[i+1]== 0xBF
                    &&buf[i+2]== 0xBD
                    )
                {
                    ret = true;
                    break;
                }
            }
            return ret;
        }

        public List<int> ReplaceInvalidChar(byte[] buf)
        {
            //ERR    EF BF BD
            //BOM:EF BB BF                                        

            List<int> errList = new List<int>();
            int i;
            for (i = 0; i < buf.Length - 3; ++i)
            {
                if (buf[i] == 0xEF
                    && buf[i + 1] == 0xBF
                    && buf[i + 2] == 0xBD
                    )
                {
                    errList.Add(i);   
                    buf[i] = 0x45;//E
                    buf[i+1] = 0x72;//r
                    buf[i+2] = 0x72;//r
                    i+=2;
                }
            }
            return errList;
        }
        /// <summary>
        /// 把输入buf中的 fix成 UTF8 串
        /// </summary>
        /// <param name="buf"></param>
        /// <returns></returns>
        public string FixBuffer(byte[] dataBuf,EncodingIndex fisrtECS = EncodingIndex.EI_UTF8,int offset =0,int len =0)
        {//分片处理
            var ecs = new Encoding[(int)EncodingIndex.EI_MAX]{

                Encoding.GetEncoding("GBK"),
                Encoding.Unicode,
                Encoding.BigEndianUnicode,
                Encoding.UTF8,
                Encoding.ASCII
                };
            if(len ==0)
            {
                len = dataBuf.Length - offset;
            }
            if(fisrtECS >= EncodingIndex.EI_MAX)
            {
                fisrtECS = EncodingIndex.EI_GBK;
            
            }


            var sb = new StringBuilder();
            int usedLen = 0;
            string theSlice;
            var theSlices = new List<string>();//记录下每个分片
            int i=0;

            EncodingIndex bomIdx = EncodingIndex.EI_MAX;
            
            //按状态机来做
            int remain ;
            int validLen = 0 ;
            bool valid ;

            for (; i < len;)
            {
                if (dataBuf[offset + i] == 0x0d || dataBuf[offset + i] == 0x0a)
                {
                    if (bomIdx == EncodingIndex.EI_MAX)
                    {
                        ++i;
                        usedLen = i;
                        continue;
                    }
                    else
                    {
                        break;
                    }
                }
                remain = len - i;
                
                
                if(bomIdx == EncodingIndex.EI_MAX)
                {
                    if(fisrtECS == EncodingIndex.EI_UTF8)
                    {
                        valid = IsValidUtf8(dataBuf, offset + i, remain, ref validLen);
                        //Debug.WriteLine($"UTF8 {i} {dataBuf[offset + i]:X2} valid:{valid} remain:{remain} len:{validLen}");
                        
                    }
                    else
                    {
                        valid = IsValidGb18030(dataBuf, offset + i, remain, ref validLen);
                        //Debug.WriteLine($"GB18030 {i} {dataBuf[offset + i]:X2} valid:{valid} remain:{remain} len:{validLen}");
                    }
                    if (valid)
                    {
                        i += validLen;
                        bomIdx = fisrtECS;
                    }
                    else
                    {
                        if (fisrtECS == EncodingIndex.EI_UTF8)
                        {
                            valid = IsValidGb18030(dataBuf, offset + i, remain, ref validLen);
                            //Debug.WriteLine($"GB18030 {i} {dataBuf[offset + i]:X2} valid:{valid} remain:{remain} len:{validLen}");
                            if(valid)
                            {
                                bomIdx = EncodingIndex.EI_GBK;
                            }
                        }
                        else
                        {
                            valid = IsValidUtf8(dataBuf, offset + i, remain, ref validLen);
                            //Debug.WriteLine($"UTF8 {i} {dataBuf[offset + i]:X2} valid:{valid} remain:{remain} len:{validLen}");
                            if (valid)
                            {
                                bomIdx = EncodingIndex.EI_UTF8;
                            }
                        }
                        //Debug.WriteLine($"UTF8 {i} {dataBuf[offset + i]:X2} valid:{valid} remain:{remain} len:{validLen}");
                        if (valid)
                        {
                            i += validLen;
                        }
                        else
                        {
                            Console.WriteLine($"Error SKIP @{i} {dataBuf[offset + i]:X2}");
                            ++i;
                        }
                    }


                }
                else
                {
                    if(bomIdx == EncodingIndex.EI_UTF8)
                    {
                        valid = IsValidUtf8(dataBuf,offset + i, remain, ref validLen);

                        //Debug.WriteLine($"UTF8 {i} {dataBuf[offset + i]:X2} valid:{valid} remain:{remain} len:{validLen}");
                        if (valid)
                        {//继续
                            i += validLen;
                        }
                        else
                        {//要结束
                            //收集片段
                            theSlice = ecs[(int)bomIdx].GetString(dataBuf,offset + usedLen, i - usedLen);
                            theSlices.Add(theSlice);//增加进去
                            sb.Append(theSlice);
                            usedLen =i;
                            //新片段
                            bomIdx = EncodingIndex.EI_MAX;
                        }
                    }
                    else if (bomIdx == EncodingIndex.EI_GBK)
                    {
                        valid = IsValidGb18030(dataBuf,offset + i, remain, ref validLen);

                        //Debug.WriteLine($"GB18030 {i} {dataBuf[offset + i]:X2} valid:{valid} remain:{remain} len:{validLen}");
                        if (valid)
                        {//继续
                            i += validLen;
                        }
                        else
                        {//要结束
                            //收集片段
                            theSlice = ecs[(int)bomIdx].GetString(dataBuf,offset + usedLen, i - usedLen);
                            theSlices.Add(theSlice);//增加进去
                            sb.Append(theSlice);
                            usedLen = i;
                            //新片段
                            bomIdx = EncodingIndex.EI_MAX;
                        }
                    }
                    else
                    {
                        throw new NotImplementedException();
                    }
                }





            }
            if (bomIdx != EncodingIndex.EI_MAX
                && i > usedLen
                )//处理最后一批
            {
                theSlice = ecs[(int)bomIdx].GetString(dataBuf,offset + usedLen, i - usedLen);
                theSlices.Add(theSlice);//增加进去
                sb.Append(theSlice);

            }

            return sb.ToString();
        }


        /// <summary>
        /// 
        /// </summary>
        /// <param name="buffer"></param>
        /// <param name="position"></param>
        /// <param name="length"></param>
        /// <param name="cnByte"></param>
        /// <returns></returns>
        public static bool IsValidUtf8Old(byte[] buffer, int position, int length, ref int cnByte)
        {
            if (position + length > buffer.Length)
            {
                throw new ArgumentException("Invalid length");
            }

            if (length < 1)
            {
                cnByte = 0;
                return false;
            }

            byte ch = buffer[position];

            if (ch <= 0x7F)
            {
                cnByte = 1;
                return true;
            }

            if (ch >= 0xc2 && ch <= 0xdf)
            {
                if (length < 2)
                {
                    cnByte = 0;
                    return false;
                }
                if (buffer[position + 1] < 0x80 || buffer[position + 1] > 0xbf)
                {
                    cnByte = 0;
                    return false;
                }
                cnByte = 2;
                return true;
            }

            if (ch == 0xe0)
            {
                if (length < 3)
                {
                    cnByte = 0;
                    return false;
                }

                if (buffer[position + 1] < 0xa0 || buffer[position + 1] > 0xbf ||
                    buffer[position + 2] < 0x80 || buffer[position + 2] > 0xbf)
                {
                    cnByte = 0;
                    return false;
                }
                cnByte = 3;
                return true;
            }


            if (ch >= 0xe1 && ch <= 0xef)
            {
                if (length < 3)
                {
                    cnByte = 0;
                    return false;
                }

                if (buffer[position + 1] < 0x80 || buffer[position + 1] > 0xbf ||
                    buffer[position + 2] < 0x80 || buffer[position + 2] > 0xbf)
                {
                    cnByte = 0;
                    return false;
                }

                cnByte = 3;
                return true;
            }

            if (ch == 0xf0)
            {
                if (length < 4)
                {
                    cnByte = 0;
                    return false;
                }

                if (buffer[position + 1] < 0x90 || buffer[position + 1] > 0xbf ||
                    buffer[position + 2] < 0x80 || buffer[position + 2] > 0xbf ||
                    buffer[position + 3] < 0x80 || buffer[position + 3] > 0xbf)
                {
                    cnByte = 0;
                    return false;
                }

                cnByte = 4;
                return true;
            }

            if (ch == 0xf4)
            {
                if (length < 4)
                {
                    cnByte = 0;
                    return false;
                }

                if (buffer[position + 1] < 0x80 || buffer[position + 1] > 0x8f ||
                    buffer[position + 2] < 0x80 || buffer[position + 2] > 0xbf ||
                    buffer[position + 3] < 0x80 || buffer[position + 3] > 0xbf)
                {
                    cnByte = 0;
                    return false;
                }

                cnByte = 4;
                return true;
            }

            if (ch >= 0xf1 && ch <= 0xf3)
            {
                if (length < 4)
                {
                    cnByte = 0;
                    return false;
                }

                if (buffer[position + 1] < 0x80 || buffer[position + 1] > 0xbf ||
                    buffer[position + 2] < 0x80 || buffer[position + 2] > 0xbf ||
                    buffer[position + 3] < 0x80 || buffer[position + 3] > 0xbf)
                {
                    cnByte = 0;
                    return false;
                }

                cnByte = 4;
                return true;
            }

            return false;
        }

        public static bool IsValidUtf8(byte[] buffer, int position, int length, ref int cnByte)
        {
            bool valid = false;
            cnByte = 0;

            if (position + length > buffer.Length || length < 1)
            {

            }
            else
            {
                byte ch0 = buffer[position];
                if ((ch0 & 0x80) ==0)
                {
                    valid = true;
                    cnByte = 1;
                }
                else // >=2
                {
                    int byCnt =0;
                    if ((ch0 & 0xe0) == 0xc0)//2 byte
                    {
                        //byCnt =2;
                        //中文不存在 2 字节的 
                        //valid = false;

                    }
                    else if ((ch0 & 0xF0) == 0xE0 && length>=3)//3 byte
                    {
                        byCnt =3;
                    }
                    else if ((ch0 & 0xF8) == 0xF0 && length >= 4)//4 byte
                    {
                        //byCnt =4;
                    }
                    else if ((ch0 & 0xFE) == 0xF8 && length >= 5)//5 byte
                    {
                        //byCnt = 5;
                    }
                    else if ((ch0 & 0xFE) == 0xFC && length >= 6)//6 byte
                    {
                        //byCnt = 6;
                    }
                    if(byCnt>0)
                    {//多字节
                        int i = 1;//后面的字节统一规则
                        for (; i < byCnt; ++i
                            )
                        {
                            if( (buffer[position + i] & 0xC0 ) != 0x80)
                            {
                                break;
                            }
                        }
                        if(i == byCnt)
                        {
                            valid = true;
                            cnByte = byCnt;
                        }
                    }


                }

            }


            return valid;
        }
        /// <summary>
        /// https://en.wikipedia.org/wiki/GBK_(character_encoding)
        /// 判断 GBK
        /// </summary>
        /// <param name="buffer"></param>
        /// <param name="position"></param>
        /// <param name="length"></param>
        /// <param name="cnByte"></param>
        /// <returns></returns>
        public static bool IsValidGbk(byte[] buffer, int position, int length, ref int cnByte)
        {
            bool valid = false;
            cnByte = 0;

            if (position + length > buffer.Length || length < 1)
            {

            }
            else
            {
                if (length == 1)
                {
                    valid = (buffer[0] & 0x80) == 0;
                    if (valid)
                    {
                        cnByte = 1;
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
                        cnByte = 2;

                    }
                }

            }


            return valid;
        }

        public static bool IsValidGb18030(byte[] buffer, int position, int length, ref int cnByte)
        {
            bool valid = false;
            cnByte = 0;

            if (position + length > buffer.Length || length < 1)
            {

            }
            else
            {
                if ((buffer[0] & 0x80) == 0)
                {
                    valid = true;
                    cnByte = 1;
                }
                else if(length>1)
                {
                    byte ch0 = buffer[position];
                    byte ch1 = buffer[position + 1];

                    if (
                        ((ch0 >= 0x81 && ch0 <= 0xFE) && (ch1 >= 0x40 && ch1 <= 0xFE && ch1 != 0x7F))
                        )
                    {
                        valid = true;
                        cnByte = 2;

                    }
                }

            }


            return valid;
        }
    }
}
