using System;
using System.Diagnostics;
using System.Text;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using utf8util;

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
            var theFixed = fix.FixBuffer(utf8,EncodingIndex.EI_UTF8);
            Assert.AreEqual(theFixed, txt);
            var gbk = Encoding.GetEncoding("GBK").GetBytes(txt);
            theFixed = fix.FixBuffer(gbk,EncodingIndex.EI_GBK);
            Assert.AreEqual(theFixed, txt);

            //var ucs2 = Encoding.Unicode.GetBytes(txt);
            //theFixed = fix.FixBuffer(ucs2);
            //Assert.AreEqual(theFixed,txt);

            //组合混合串 顺序1
//             var mixedBuf = new byte[utf8.Length + gbk.Length + utf8.Length];
//             var theMixedStr = txt + txt + txt;
//             int iOffset = 0;
// 
//             Buffer.BlockCopy(utf8, 0, mixedBuf, iOffset, utf8.Length);
//             iOffset += utf8.Length;
// 
//             Buffer.BlockCopy(gbk, 0, mixedBuf, iOffset, gbk.Length);
//             iOffset += gbk.Length;
// 
//             Buffer.BlockCopy(utf8, 0, mixedBuf, iOffset, utf8.Length);
//             iOffset += utf8.Length;
// 
//             theFixed = fix.FixBuffer(mixedBuf);
//             Assert.AreEqual(theFixed, theMixedStr);
// 
//             //顺序2
//             mixedBuf = new byte[gbk.Length + gbk.Length + utf8.Length];
//             iOffset = 0;
// 
//             Buffer.BlockCopy(gbk, 0, mixedBuf, iOffset, gbk.Length);
//             iOffset += gbk.Length;
// 
//             Buffer.BlockCopy(utf8, 0, mixedBuf, iOffset, utf8.Length);
//             iOffset += utf8.Length;
// 
//             Buffer.BlockCopy(gbk, 0, mixedBuf, iOffset, gbk.Length);
//             iOffset += gbk.Length;
// 
// 
//             theFixed = fix.FixBuffer(mixedBuf);
//             Assert.AreEqual(theFixed, theMixedStr);
// 

        }
        [TestMethod]
        public void FixMixedText()
        {
            var mixedData = new byte[]{
    0x2F, 0x2F, 0x70, 0x72, 0x69, 0x6E, 0x74, 0x66, 0x28, 0x22,

    0x31, 0x3A, 0x20, 0xD5,0xFD, 0xD4, 0xDA, 0xBD, 0xE2, 0xCE, 0xF6, 0xCF, 0xC2, 0xD4, 0xD8, 0xB5, 0xD8, 0xD6, 0xB7, 0x2E,0x2E, 0x2E, 
/*
UTF8:
31-3A-20-E6-AD-A3-E5-9C-A8-E8-A7-A3-E6-9E-90-E4-B8-8B-E8-BD-BD-E5-9C-B0-E5-9D-80-2E-2E-2E
GBK:
31-3A-20-D5-FD-D4-DA-BD-E2-CE-F6-CF-C2-D4-D8-B5-D8-D6-B7-2E-2E-2E
UCS2:
31-00-3A-00-20-00-63-6B-28-57-E3-89-90-67-0B-4E-7D-8F-30-57-40-57-2E-00-2E-00-2E-00		

0x31,0x3a,0x20,0xef,0xbf,0xbd,0xef,0xbf,0xbd,0xef,0xbf,0xbd,0xda,0xbd,0xef,0xbf,0xbd,0xef,0xbf,0xbd,0xef,0xbf,0xbd,0xef,0xbf,0xbd,0xef,
0xbf,0xbd,0xef,0xbf,0xbd,0xd8,0xb5,0xef,0xbf,0xbd,0xd6,0xb7,
*/
    
    0x22, 0x29, 0x3B,
};

            var fix = new utf8util.utf8fix();
            var text = fix.FixBuffer(mixedData);

            Assert.AreEqual(text, "//printf(\"1: 正在解析下载地址...\");");


            var data2 = new byte[]
            {
                0x2F, 0x2F, 0xC1, 0xAC, 0xBD, 0xD3, 0xD4, 0xB6, 0xB3, 0xCC, 0xD6, 0xF7, 0xBB, 0xFA,
/*
 UTF8:
2F-2F-E8-BF-9E-E6-8E-A5-E8-BF-9C-E7-A8-8B-E4-B8-BB-E6-9C-BA
      EF BF BD  -- UTF8 的非法字数据  
GBK:
2F-2F-C1-AC-BD-D3-D4-B6-B3-CC-D6-F7-BB-FA
UCS2:
2F-00-2F-00-DE-8F-A5-63-DC-8F-0B-7A-3B-4E-3A-67


                 */
            };

            text = fix.FixBuffer(data2, EncodingIndex.EI_UTF8);
            var textFromGbk = fix.FixBuffer(data2, EncodingIndex.EI_GBK);

            var dataK = Encoding.UTF8.GetBytes(text);

            //Assert.IsFalse(fix.HasInvalidChar(dataK));
            //if(dataK.)
            //Assert.AreEqual(text, "//连接远程主机");

            data2 = new byte[] { 0xD4, 0xB6, 0xB3, 0xCC, 0xD6, 0xF7, 0xBB, 0xFA, };
            text = fix.FixBuffer(data2, EncodingIndex.EI_UTF8);
            textFromGbk = fix.FixBuffer(data2, EncodingIndex.EI_GBK);
            dataK = Encoding.UTF8.GetBytes(text);
            var dataGbk = Encoding.UTF8.GetBytes(textFromGbk);
            Debug.WriteLine("UTF8:" + BitConverter.ToString(dataK));
            Debug.WriteLine("GBK:" + BitConverter.ToString(dataGbk));
            Debug.WriteLine("GBK:" + BitConverter.ToString(dataGbk));
            //Assert.IsFalse(fix.HasInvalidChar(dataK));
            //if(dataK.)
            //Assert.AreEqual(text, "远程主机");

        }

        /// <summary>
        /// 测试混合数据的修正
        /// </summary>
        [TestMethod]
        public void FixReplace()
        {
            var mixedData = new byte[]{
    0x0A, 0x2F, 0x2F, 0xD6, 0xA7, 0xEF, 0xBF, 0xBD, 0xEF, 0xBF, 0xBD, 0xEF, 0xBF, 0xBD, 0xEF, 0xBF,
    0xBD, 0xEF, 0xBF, 0xBD, 0xEF, 0xBF, 0xBD, 0x67, 0x70, 0x69, 0x6F, 0xEF, 0xBF, 0xBD, 0xEF, 0xBF,
    0xBD, 0xCA, 0xBC, 0xEF, 0xBF, 0xBD, 0xEF, 0xBF, 0xBD, 0xEF, 0xBF, 0xBD, 0xEF, 0xBF, 0xBD, 0x28,
    0x65, 0x65, 0x70, 0x29, 0x0D, 0x0A,

            };

            var fix = new utf8util.utf8fix();

            Assert.IsTrue(fix.HasInvalidChar(mixedData));

            fix.ReplaceInvalidChar(mixedData);
            Assert.IsFalse(fix.HasInvalidChar(mixedData));


            var text = fix.FixBuffer(mixedData, EncodingIndex.EI_UTF8);
            var data = Encoding.UTF8.GetBytes(text);

            Assert.IsFalse(fix.HasInvalidChar(data));
        }



    }
}
