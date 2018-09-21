using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

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
                var ff = System.IO.Directory.EnumerateFiles(args[0], args[1], SearchOption.AllDirectories).ToArray();
                Console.WriteLine($"Total Files: {ff.Length}");
                var fix = new utf8util.utf8fix();
                int ln =0;
                var gbkEcs = Encoding.GetEncoding("GBK"));
                foreach (var f in ff)
                {
                    //UTF8的部分 这个可以修回来
                    //GBK的部分，要用python
                    //或者 python 按行转为UTF8
                    //这个纠 UTF8的
                    var lo = new List<string>();
                    ln =0;
                    var oo = System.IO.File.ReadAllLines(f);//, Encoding.GetEncoding("GBK"));
                    //按行来做处理
                    foreach(var line in oo)
                    {
                        var utf8 = Encoding.UTF8.GetBytes(line);
                        var gbk = gbkEcs.GetBytes(line);
                        ++ln;
                        if(ln == 330)
                        {
                            Console.WriteLine($"{ln}");
                        }

                        var fixedLen = fix.FixBuffer(utf8);
                        //
                        
                        lo.Add(fixedLen);
                    }
                    System.IO.File.WriteAllLines(f, lo, Encoding.UTF8);
                }
            }
        }
    }
}
