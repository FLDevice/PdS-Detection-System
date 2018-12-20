using MySql.Data.MySqlClient;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace DetectionSystem
{
    /// <summary>
    /// Logica di interazione per Home.xaml
    /// </summary>
    public partial class Home : Page
    {
        protected Process TCPServer;
        protected MySqlConnection DBconnection;

        protected String filename;

        public static TextBox output_box;
        public static TextBox espn_box;

        protected bool is_running = false;

        public Home()
        {
            InitializeComponent();
            output_box = (TextBox)this.FindName("stdout");
            espn_box = (TextBox)this.FindName("esp_number");
           
            try
            {
                DBconnection = new MySqlConnection();
                DBconnection.ConnectionString = "server=localhost; database=pds_db; uid=pds_user; pwd=password";
                DBconnection.Open();
                MySqlCommand cmm = new MySqlCommand("select count(*) from devices", DBconnection);
                MySqlDataReader r = cmm.ExecuteReader();
                while (r.Read()) {
                    output_box.AppendText(""+r[0]);
                }
            }
            catch (Exception e)
            {
                output_box.AppendText(e.Message);
            }
            

        }

        private void Button_Click(object sender, RoutedEventArgs e)
        {
            if (filename == null) return;

            if (is_running == false)
            {
                is_running = true;
            
                // View Expense Report
                TCPServer = new Process();
                TCPServer.StartInfo.FileName = filename;

                string num = espn_box.Text;
                string x0 = esp0x.Text, y0 = esp0y.Text;
                string x1 = esp1x.Text, y1 = esp1y.Text;
                string x2 = esp2x.Text, y2 = esp2y.Text;
                string x3 = esp3x.Text, y3 = esp3y.Text;
                string x4 = esp4x.Text, y4 = esp4y.Text;
                string x5 = esp5x.Text, y5 = esp5y.Text;
                string x6 = esp6x.Text, y6 = esp6y.Text;
                string x7 = esp7x.Text, y7 = esp7y.Text;
                TCPServer.StartInfo.Arguments = num + " " + x0 + " " + y0 + " " + x1 + " " + y1 + " " +
                                                            x2 + " " + y2 + " " + x3 + " " + y3 + " " +
                                                            x4 + " " + y4 + " " + x5 + " " + y5 + " " +
                                                            x6 + " " + y6 + " " + x7 + " " + y7;
                TCPServer.StartInfo.UseShellExecute = false;
                TCPServer.StartInfo.CreateNoWindow = true;
                TCPServer.StartInfo.RedirectStandardOutput = true;
                TCPServer.StartInfo.RedirectStandardError = true;
                TCPServer.OutputDataReceived += new DataReceivedEventHandler(TCPServerOutputHandler);

                TCPServer.Start();
                TCPServer.BeginOutputReadLine();             
            }
        }

        /** Print TCPServer output in a TextBox */
        private void TCPServerOutputHandler(object sendingProcess, DataReceivedEventArgs outLine) {
            if (!String.IsNullOrEmpty(outLine.Data)) {
                Application.Current.Dispatcher.Invoke(() => {
                    output_box.AppendText(outLine.Data + "\n");
                    output_box.ScrollToEnd();
                });
            }
        }

        /** Kill TCPServer if the main window is terminating */
        public void OnWindowClosing(object sender, System.ComponentModel.CancelEventArgs e) {
            if (is_running) {
                // if already terminated do nothing
                if (TCPServer.ProcessName.Length == 0) return;

                TCPServer.Kill();
                TCPServer.WaitForExit();
            }
        }

        private void Exe_picker_Click(object sender, RoutedEventArgs e)
        {
            Microsoft.Win32.OpenFileDialog dlg = new Microsoft.Win32.OpenFileDialog();
            dlg.Filter = "Executable File (.exe) | *.exe";
            Nullable<bool> result = dlg.ShowDialog();

            if (result == true) {
                filename = dlg.FileName; 
            }
        }
    }
}
