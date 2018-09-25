using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using utf8util;

namespace FixMacro
{
    class Program
    {
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
                var pat = args[1].Split(new char[] { ';', ',' }, StringSplitOptions.RemoveEmptyEntries);
                var ff = DirUtil.GetFiles(args[0], pat, SearchOption.AllDirectories).OrderBy(L => L).ToArray();
                Console.WriteLine($"Total Files: {ff.Length}");
                var fix = new utf8util.utf8fix();
                int ln = 0;

                var gbkEcs = Encoding.GetEncoding("GBK");
                bool changed;
                foreach (var f in ff)
                {
                    //UTF8的部分 这个可以修回来
                    //GBK的部分，要用python
                    //或者 python 按行转为UTF8
                    //这个纠 UTF8的
                    //var lo = new List<string>();
                    ln = 0;
                    var oo = System.IO.File.ReadAllLines(f);
                    int i;
                    string theLn;
                    changed = false;
                    for(i=0;i<oo.Length;++i)
                    {
                        theLn = oo[i].Trim().Replace("\t","").Replace(" ","");
                        if(theLn.Length ==0 && i>1 && oo[i-1].EndsWith("\\"))
                        {
                            oo[i-1] = oo[i-1].TrimEnd('\\');
                            if(!changed)
                            {
                                changed = true;
                            }
                        }
                    }
                    if(changed)
                    {
                        Console.WriteLine($"{f} changed");
                        System.IO.File.WriteAllLines(f, oo, Encoding.UTF8);

                    }
                }
                Console.WriteLine("Done");
            }
        }
    }
}
