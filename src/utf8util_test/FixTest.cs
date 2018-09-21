using System;
using System.Text;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace utf8util_test
{
    [TestClass]
    public class FixTest
    {
        /// <summary>
        /// 自动生成数据来做混合测试
        /// </summary>
        [TestMethod]
        public void FixGenText()
        {
            var txt = "123张三b李四a";
            var fix = new utf8util.utf8fix();

            var utf8 = Encoding.UTF8.GetBytes(txt);
            var theFixed = fix.FixBuffer(utf8);
            Assert.AreEqual(theFixed,txt);
            var gbk = Encoding.GetEncoding("GBK").GetBytes(txt);
            theFixed = fix.FixBuffer(gbk);
            Assert.AreEqual(theFixed,txt);

            //var ucs2 = Encoding.Unicode.GetBytes(txt);
            //theFixed = fix.FixBuffer(ucs2);
            //Assert.AreEqual(theFixed,txt);

            //组合混合串 顺序1
            var mixedBuf = new byte[utf8.Length + gbk.Length + utf8.Length];
            var theMixedStr = txt + txt + txt;
            int iOffset =0;

            Buffer.BlockCopy(utf8,0,mixedBuf,iOffset,utf8.Length);
            iOffset += utf8.Length;

            Buffer.BlockCopy(gbk, 0, mixedBuf, iOffset, gbk.Length);
            iOffset += gbk.Length;

            Buffer.BlockCopy(utf8, 0, mixedBuf, iOffset, utf8.Length);
            iOffset += utf8.Length;

            theFixed = fix.FixBuffer(mixedBuf);
            Assert.AreEqual(theFixed, theMixedStr);

            //顺序2
            mixedBuf = new byte[gbk.Length + gbk.Length + utf8.Length];
            iOffset = 0;

            Buffer.BlockCopy(gbk, 0, mixedBuf, iOffset, gbk.Length);
            iOffset += gbk.Length;

            Buffer.BlockCopy(utf8, 0, mixedBuf, iOffset, utf8.Length);
            iOffset += utf8.Length;

            Buffer.BlockCopy(gbk, 0, mixedBuf, iOffset, gbk.Length);
            iOffset += gbk.Length;


            theFixed = fix.FixBuffer(mixedBuf);
            Assert.AreEqual(theFixed, theMixedStr);


        }
        /// <summary>
        /// 测试混合数据的修正
        /// </summary>
        //[TestMethod]
        //public void FixMixedText()
        //{
        //    var mixedData = new byte[]{
        //        0xEF,0xBB,0xBF, //BOM
        //        //GBK 字符串 //字符串处理
        //        0x2F, 0x2F, 0xE5, 0xAD, 0x97, 0xE7, 0xAC, 0xA6, 0xE4, 0xB8, 0xB2, 0xE5, 0xA4, 0x84, 0xE7, 0x90,
        //        0x86, 0x0A,

        //        //UTF8 
        //        0x70, 0x72, 0x69, 0x6E, 0x74, 0x66, 0x28, 0x22, 0x31, 0x3A, 0x20, 0xD5, 0xFD, 0xD4, 0xDA, 0xBD,
        //        0xE2, 0xCE, 0xF6, 0xCF, 0xC2, 0xD4, 0xD8, 0xB5, 0xD8, 0xD6, 0xB7, 0x2E, 0x2E, 0x2E, 0x22, 0x29,
        //        0x3B,

        //        //UCS2
        //        0x31,0x00,0x32,0x00,0x33,0x00,0x20,0x5F,0x09,0x4E,0x62,0x00,0x4E,0x67,0xDB,0x56,0x61,0x00

        //        };

        //    var fix = new utf8util.utf8fix();
        //    var text = fix.FixBuffer(mixedData);

        //    Assert.AreEqual(text,"");

        //}

    }
}
