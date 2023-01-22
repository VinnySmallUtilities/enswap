using System;
using System.IO;
using System.Text.RegularExpressions;
using System.Collections.Generic;
using System.Threading;
using System.Diagnostics;

namespace enswap;
class MainClass
{
    static long ParseValue(string line)
    {
        long result = 0;
        int i = 0;
        for (; line[i] != ':'; i++) {}
        for (; line[i] < '0'  || line[i] > '9';  i++) {}
        for (; line[i] >= '0' && line[i] <= '9'; i++)
            result = result * 10  + (int)(line[i] - '0');
            
        return result;
    }
    
    static string[] captureStrs = new string[] { "MemTotal:", "MemAvailable:", "SwapTotal:" };
    static Random   rnd         = new Random();
    
    const long      minAvailableMemory = 1024*1024 * 3/2; // in kilobytes 1024*1024 = 1 Gb
    const int       intervalForCheck   = 500;
    public static int Main(string[] args)
    {
        if (args.Length != 1 && args.Length != 2)
        {
            Console.Error.WriteLine("Must be 1 or 2 arguments: script path for execute and script parameters in quotes");
            Console.Error.WriteLine("Example");
            Console.Error.WriteLine("/media/veracrypt1/swap.sh \"2G\"");

            return 1;
            // args = new string[] { "/media/veracrypt1/swap.sh", "2G" };
        }

        var exec = true;
        var Dict = new Dictionary<string, long>(2);
        Console.CancelKeyPress += (sender, e) => exec = false;

        while (exec)
        {
            Dict.Clear();
            Thread.Sleep(rnd.Next(50, intervalForCheck));

            // System.Diagnostics.Process.Start("free");
            var memInfoLines = File.ReadAllLines("/proc/meminfo");

            foreach (var line in memInfoLines)
            {
                foreach (var captureStr in captureStrs)
                    if (line.StartsWith(captureStr, StringComparison.InvariantCulture))
                    {
                        Dict.Add(captureStr, ParseValue(line));
                    }

                if (Dict.Count >= 3)
                    break;
            }

            if (Dict[captureStrs[1]] <= minAvailableMemory)
            {
                // if (Dict[captureStrs[2]] <= 0)

                Process pi;
                if (args.Length == 1)
                    pi = System.Diagnostics.Process.Start(args[0]);
                else
                    pi = System.Diagnostics.Process.Start(args[0], args[1]);

                pi.WaitForExit();

                return 0;
            }
        }
        
        return 0;
    }
}

