﻿using System;
using System.Collections.Generic;
using System.Configuration;
using System.Windows;

namespace OpenHome.Soundcard
{
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
        protected override void OnDeactivated(EventArgs e)
        {
            Console.WriteLine("Application deactivated");

            base.OnDeactivated(e);

            MainWindow main = MainWindow as MainWindow;

            main.ApplicationDeactivated();
        }        
    }
}
