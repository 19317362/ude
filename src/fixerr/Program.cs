﻿using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using utf8util;

namespace fixerr
{
    class Program
    {
        /// <summary>
        /// 修正错误的编码文件
        /// </summary>
        /// <param name="args"></param>
        static void Main(string[] args)
        {
            if (args.Length < 2)
            {
                Console.WriteLine("Usage addbom <path> <*.cpp;*.c;*.h>");
            }
            else
            {

                //Console.WriteLine("UTF8:\r\n{0}", BitConverter.ToString(utf8));
                //Console.WriteLine("GBK:\r\n{0}", BitConverter.ToString(gbk));
                //Console.WriteLine("UCS2:\r\n{0}", BitConverter.ToString(ucs2));
                var pat = args[1].Split(new char[]{';',',' },StringSplitOptions.RemoveEmptyEntries);
                var ff = DirUtil.GetFiles(args[0], pat, SearchOption.AllDirectories).ToArray();
                Console.WriteLine($"Total Files: {ff.Length}");
                var fix = new utf8util.utf8fix();
                int ln =0;
                int donePos=0;
                int i;
                
                var gbkEcs = Encoding.GetEncoding("GBK");
                foreach (var f in ff)
                {
                    //UTF8的部分 这个可以修回来
                    //GBK的部分，要用python
                    //或者 python 按行转为UTF8
                    //这个纠 UTF8的
                    var lo = new List<string>();
                    ln =0;
                    var oo = System.IO.File.ReadAllBytes(f);//, Encoding.GetEncoding("GBK"));
                    //判断有无 0x00 UCS2 ...
                    var haveUcs2 = oo.Any(L => L == 0x00);
                    if(haveUcs2)
                    {
                        Console.WriteLine($"ERROR_1 UCS2 {haveUcs2} {f}");
                        continue;
                    }
                    EncodingIndex bomIdx = EncodingIndex.EI_UTF8;
                    i = (oo.Length >= 3 && oo[0] == 0xEF && oo[0 + 1] == 0xBB && oo[0 + 2] == 0xBF) ? 3 : 0;
                    if (i == 0)
                    {
                        if (oo.Length >= 2)
                        {
                            if (oo[0 + 0] == 0xFF && oo[0 + 1] == 0xFE)//UCS2-LE
                            {//应该主要是这种方式
                                
                                i = 2;
                                bomIdx = EncodingIndex.EI_UCS2_LE;
                                Console.WriteLine($"ERROR_2{bomIdx} {f}");
                                continue;
                            }
                            else if (oo[0 + 0] == 0xFE && oo[0 + 1] == 0xFF)//UCS2-BE
                            {
                                bomIdx = EncodingIndex.EI_UCS2_BE;
                                i = 2;
                                Console.WriteLine($"ERROR_3 {bomIdx} {f}");
                                continue;
                            }

                        }

                    }
                    else
                    {//已经是UTF8-BOM
                        bomIdx = EncodingIndex.EI_UTF8;
                        if(fix.HasInvalidChar(oo))
                        {
                            Console.WriteLine($"ERROR_2 UTF8-BOM INVALID {f}");
                        }
                        else
                        {
                            Console.WriteLine($"OK UTF8-BOM VALID {f}");
                            continue;
                        }
                    }
                    

                    //按行来做处理
                    //foreach(var line in oo)
                    for (donePos =i; i< oo.Length;++i )
                    {
                        if(oo[i] == 0x0a || oo[i] == 0x0d)
                        {//新的一行到了
                            if((i-1)> donePos)
                            {

                                ++ln;

                                var fixedLn = fix.FixBuffer(oo,bomIdx,donePos,i - donePos);
                                                                                     
                                var dataK = Encoding.UTF8.GetBytes(fixedLn);
                                //if (ln == 347)
                                //{
                                //    Console.WriteLine($"{ln} OFFSET:{donePos:X}");
                                //    Console.WriteLine("ORG:" + BitConverter.ToString(oo,donePos,i-donePos));
                                //    Console.WriteLine("AFT:" + BitConverter.ToString(dataK));
                                //}
                                
                                if (fix.HasInvalidChar(dataK))
                                {
                                    Console.WriteLine($"{ln} OFFSET:{donePos:X}");
                                    Console.WriteLine("ORG:" + BitConverter.ToString(oo, donePos, i - donePos));
                                    Console.WriteLine("AFT:" + BitConverter.ToString(dataK));

                                    EncodingIndex alterDcs;
                                    if(bomIdx == EncodingIndex.EI_UTF8)
                                    {
                                        alterDcs =EncodingIndex.EI_GBK;
                                    }
                                    else
                                    {
                                        alterDcs =EncodingIndex.EI_UTF8;
                                    }
                                    fixedLn = fix.FixBuffer(oo, alterDcs, donePos, i - donePos);
                                    dataK = Encoding.UTF8.GetBytes(fixedLn);
                                    Console.WriteLine("AFT2:" + BitConverter.ToString(dataK));
                                    if (fix.HasInvalidChar(dataK))
                                    {
                                        Console.WriteLine($"ERROR_5 {ln} {alterDcs} INVALID {f}");
                                    }
                                    else
                                    {
                                        Console.WriteLine($"WARNING_1 {ln} {bomIdx} INVALID  {alterDcs} OK {f}");
                                    }
                                }
                                lo.Add(fixedLn);
                                donePos = i;

                            }
                            else
                            {
                                donePos =i;
                            }
                        }

                        //var utf8 = Encoding.UTF8.GetBytes(line);
                        //var gbk = gbkEcs.GetBytes(line);
                    }
                    System.IO.File.WriteAllLines(f, lo, Encoding.UTF8);
                }
            }
        }
    }
}
