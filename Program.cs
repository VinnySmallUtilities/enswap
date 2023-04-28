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
    
    static string[] captureStrs = new string[] { "MemTotal:", "MemAvailable:", "SwapTotal:", "SwapFree:" };
    static Random   rnd         = new Random();

    const  string    errorFileName        = "error.log";
    static int       baseIntervalForCheck = 500;
    static int       intervalForCheck     = baseIntervalForCheck;
    public static int Main(string[] args)
    {
        if (args.Length != 1)
        {
            Console.Error.WriteLine("Must be 1 argument: options file path");
            Console.Error.WriteLine("If options file not exists, it will be created");
            Console.Error.WriteLine("Example");
            Console.Error.WriteLine("enswap");

            return 1;
            // args = new string[] { "/media/veracrypt1/swap.sh", "2G" };
        }

        var optionsFileName = new FileInfo(args[0]);
        optionsFileName.Refresh();
        if (!optionsFileName.Exists)
        {
            File.WriteAllText(optionsFileName.FullName, "4G\n1536\n/Arcs/swapfile\n/usr/local/sbin/sdel\n");
            Console.Error.WriteLine("Options file created. See it and let's correct");

            return 2;
        }

        Console.WriteLine($"strating with options file '{optionsFileName.FullName}'");

        var optionsStrings = File.ReadAllLines(optionsFileName.FullName);
        var swapL = optionsStrings[0];
        var freeM = long.Parse(optionsStrings[1]) * 1024;
        var swapF = optionsStrings[2];
        var sdel  = optionsStrings[3];

        baseIntervalForCheck = Int32.Parse(optionsStrings[4]);
        intervalForCheck     = baseIntervalForCheck;

        deleteAllSwaps(swapF, sdel);

        var exec = true;
        var Dict = new Dictionary<string, long>(2);
        Console.CancelKeyPress += (sender, e) => exec = false;
        AppDomain.CurrentDomain.ProcessExit += (sender, e) => exec = false;

        Console.WriteLine($"strated. '{swapL}'; '{freeM}'; '{swapF}'; '{sdel}'; {baseIntervalForCheck}.");

        bool outputStatisticToConsole = false;

        while (exec)
        {
            outputStatisticToConsole = ProcessMemory();
            GC.Collect();
        }

        Console.WriteLine("exiting");
        var pid = Process.Start("swapoff", "-a");
        pid.WaitForExit();

        deleteAllSwaps(swapF, sdel);

        return 0;

        bool ProcessMemory()
        {
            Dict.Clear();
            Thread.Sleep(intervalForCheck + rnd.Next(0, 56));
            if (!exec)
                return false;

            // System.Diagnostics.Process.Start("free");
            var memInfoLines = File.ReadAllLines("/proc/meminfo");

            foreach (var line in memInfoLines)
            {
                foreach (var captureStr in captureStrs)
                    if (line.StartsWith(captureStr, StringComparison.InvariantCulture))
                    {
                        Dict.Add(captureStr, ParseValue(line));
                        break;
                    }

                if (Dict.Count >= captureStrs.Length)
                    break;
            }

            var totalFree = Dict[captureStrs[1]] + Dict[captureStrs[3]];
            if (!outputStatisticToConsole)
            {
                Console.WriteLine($"Statistic of memory: available {Dict[captureStrs[1]]}; total free: {totalFree}; {Dict[captureStrs[3]]}; {totalFree}; require {freeM}");
                outputStatisticToConsole = true;
            }

            if (totalFree <= freeM)
            {
                Console.WriteLine($"Low memory: {Dict[captureStrs[1]]}; {Dict[captureStrs[3]]}; {totalFree} <= {freeM}");
                // if (Dict[captureStrs[2]] <= 0)
                var newSwapFile = getFile(swapF, swapL, sdel);
                swapFiles.Add(newSwapFile);

                outputStatisticToConsole = false;
                Thread.Sleep(intervalForCheck);

                intervalForCheck = baseIntervalForCheck >> 2;
            }
            else
            {
                var ifc = 0;
                // Если осталось меньше половины запаса от freeM
                if (totalFree - (freeM >> 1) <= freeM)
                {
                    ifc = baseIntervalForCheck >> 2;
                }
                else
                // Если осталось меньше двух запасов
                if (totalFree <= freeM << 1)
                {
                    ifc = baseIntervalForCheck >> 1;
                }
                else
                // Если осталось больше двух с половиной запасов
                if (totalFree - (freeM >> 1) <= freeM << 1)
                {
                    ifc = baseIntervalForCheck;
                }
                else
                    ifc = baseIntervalForCheck;


                if (ifc != 0)
                    if (intervalForCheck != ifc)
                    {
                        Console.WriteLine($"new interval: {ifc}; old: {intervalForCheck}");
                        intervalForCheck = ifc;
                    }
            }

            return outputStatisticToConsole;
        }
    }

    static void deleteAllSwaps(string pathTemplate, string sdel_path)
    {
        Process? pi;
        for (int i = swapFiles.Count; i < 100; i++)
        {
            try
            {
                var fi = new FileInfo(pathTemplate + "-" + i.ToString("D2"));
                fi.Refresh();
                if (fi.Exists)
                {
                    Console.WriteLine($"try to delete file {fi.FullName}");
                    pi = Process.Start(sdel_path, $"- {fi.FullName}");
                    pi.WaitForExit();
                }
                else
                    continue;

                fi.Refresh();
                if (fi.Exists)
                {
                    var err = $"swap files can not delete with template '{pathTemplate}' {DateTime.Now.ToString()}";
                    Console.WriteLine(err);
                    File.AppendAllText(errorFileName, err + "\n");
                }
                else
                    Console.WriteLine($"file deleted: {fi.FullName}");
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.Message);
                File.AppendAllText(errorFileName, ex.Message + "\n");
            }
        }
    }

    static List<FileInfo> swapFiles = new List<FileInfo>();
    static FileInfo getFile(string pathTemplate, string swapFileLenParam, string sdel_path)
    {
        Process? pi;
        for (int i = swapFiles.Count; i < 3; i++)
        {
            var fi = new FileInfo(pathTemplate + "-" + i.ToString("D2"));
            fi.Refresh();
            try
            {
                if (fi.Exists)
                {
                    pi = Process.Start(sdel_path, $"- {fi.FullName}");
                    pi.WaitForExit();
                }

                fi.Refresh();
                if (fi.Exists)
                    continue;
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.Message);
                File.AppendAllText(errorFileName, ex.Message + "\n");
                continue;
            }

            pi = Process.Start("fallocate", $"-l {swapFileLenParam} {fi.FullName}");
            pi.WaitForExit();

            fi.Refresh();
            if (!fi.Exists)
            {
                var errf = $"swap files can not created with template '{pathTemplate}' [fallocate problem] {DateTime.Now.ToString()}";
                File.AppendAllText(errorFileName, errf + "\n");
                throw new Exception(errf);
            }

            pi = Process.Start(fileName: "chmod", $"0600 {fi.FullName}");
            pi.WaitForExit();

            pi = Process.Start(fileName: "mkswap", $"{fi.FullName}");
            pi.WaitForExit();

            pi = Process.Start(fileName: "swapon", $"-v {fi.FullName}");
            pi.WaitForExit();

            Console.WriteLine($"Swap file created: {fi.FullName}");

            return fi;
        }

        var err = $"swap files can not created with template '{pathTemplate}' {DateTime.Now.ToString()}";
        File.AppendAllText(errorFileName, err + "\n");
        throw new Exception(err);
    }
}
