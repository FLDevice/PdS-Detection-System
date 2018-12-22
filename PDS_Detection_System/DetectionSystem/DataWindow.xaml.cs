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
using System.Windows.Shapes;

namespace DetectionSystem
{
    /// <summary>
    /// Logica di interazione per DataWindow.xaml
    /// </summary>
    public partial class DataWindow : Window
    {
        protected Process TCPServer;
        protected MySqlConnection DBconnection;

        private string fileN;
        private string args;

        public static TextBox output_box;

        protected bool is_running = false;


        public DataWindow(string args, string fileN)
        {
            InitializeComponent();
            this.args = args;
            this.fileN = fileN;

            Closing += OnWindowClosing;
            output_box = (TextBox)this.FindName("stdout2");
            StartServer();
            try
            {
                DBconnection = new MySqlConnection();
                DBconnection.ConnectionString = "server=localhost; database=pds_db; uid=pds_user; pwd=password";
                DBconnection.Open();
                MySqlCommand cmm = new MySqlCommand("select count(*) from devices", DBconnection);
                MySqlDataReader r = cmm.ExecuteReader();
                while (r.Read())
                {
                    output_box.AppendText("" + r[0]);
                }
            }
            catch (Exception e)
            {
                output_box.AppendText(e.Message + "\n");
                output_box.ScrollToEnd();
            }

        }


        private void StartServer() {
            is_running = true;
            if (fileN == null)  {
                output_box.AppendText("ERROR: Filename null");
                return;
            }
            // View Expense Report
            TCPServer = new Process();
            
            TCPServer.StartInfo.FileName = fileN;
            TCPServer.StartInfo.Arguments = args;            
            TCPServer.StartInfo.UseShellExecute = false;
            TCPServer.StartInfo.CreateNoWindow = true;
            TCPServer.StartInfo.RedirectStandardOutput = true;
            TCPServer.StartInfo.RedirectStandardError = true;
            TCPServer.OutputDataReceived += new DataReceivedEventHandler(TCPServerOutputHandler);
            TCPServer.Start();            
            TCPServer.BeginOutputReadLine();
        }


        /** Print TCPServer output in a TextBox */
        private void TCPServerOutputHandler(object sendingProcess, DataReceivedEventArgs outLine)
        {
            if (!String.IsNullOrEmpty(outLine.Data))
            {
                Application.Current.Dispatcher.Invoke(() => {
                    output_box.AppendText(outLine.Data + "\n");
                    output_box.ScrollToEnd();
                });
            }
        }



        /** Kill TCPServer if the main window is terminating */
        public void OnWindowClosing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            if (is_running)
            {
                // if already terminated do nothing
                if (TCPServer.ProcessName.Length == 0) return;

                TCPServer.Kill();
                TCPServer.WaitForExit();
            }
        }



    }
}
