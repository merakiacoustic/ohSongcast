using System;
using System.Net;

using OpenHome.Songcaster;

    class Program : IReceiverHandler, ISubnetHandler, IConfigurationChangedHandler
    {
        public static void Main(string[] args)
        {
            Program program = new Program();

            program.Run();
        }

        public void Run()
        {
            bool enabled = true;

            try
            {
                Songcaster songcaster = new Songcaster("av.openhome.org", 522, 1, 1, 100, false, enabled, 99, this, this, this, "OpenHome", "http://www.openhome.org", "http://www.openhome.org");

                while (true)
                {
                    ConsoleKeyInfo key = Console.ReadKey(true);

                    if (key.KeyChar == 'q')
                    {
                        break;
                    }

                    if (key.KeyChar == 'a')
                    {
                        songcaster.SetSubnet(43200);
                    }

                    if (key.KeyChar == 'e')
                    {
                        if (enabled)
                        {
                            enabled = false;
                        }
                        else
                        {
                            enabled = true;
                        }

                        songcaster.SetEnabled(enabled);

                        continue;
                    }
                }

                songcaster.Dispose();
            }
            catch (SongcasterError e)
            {
                Console.WriteLine(e.Message);
            }
        }

        public void ConfigurationChanged(IConfiguration aConfiguration)
        {
            Console.WriteLine("Configuration changed: channel={0}, ttl={1}, multicast={2}, enabled={3}", aConfiguration.Channel(), aConfiguration.Ttl(), aConfiguration.Multicast(), aConfiguration.Enabled());
        }

        public void ReceiverAdded(IReceiver aReceiver)
        {
            Console.WriteLine("Added   {0}:{1}:{2}:{3}", aReceiver.Room, aReceiver.Group, aReceiver.Name, aReceiver.Status);
        }

        public void ReceiverChanged(IReceiver aReceiver)
        {
            Console.WriteLine("Changed {0}:{1}:{2}:{3}", aReceiver.Room, aReceiver.Group, aReceiver.Name, aReceiver.Status);
        }

        public void ReceiverRemoved(IReceiver aReceiver)
        {
            Console.WriteLine("Removed {0}:{1}:{2}:{3}", aReceiver.Room, aReceiver.Group, aReceiver.Name, aReceiver.Status);
        }

        public void SubnetAdded(ISubnet aSubnet)
        {
            Console.WriteLine("Added   {0}:{1}", aSubnet.Address, aSubnet.AdapterName);
        }

        public void SubnetChanged(ISubnet aSubnet)
        {
            Console.WriteLine("Changed {0}:{1}", aSubnet.Address, aSubnet.AdapterName);
        }

        public void SubnetRemoved(ISubnet aSubnet)
        {
            Console.WriteLine("Removed {0}:{1}", aSubnet.Address, aSubnet.AdapterName);
        }
    }
