using MySql.Data.MySqlClient;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Pipes;
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
using LiveCharts;
using LiveCharts.Wpf;
using LiveCharts.Defaults;
using System.Threading;


namespace DetectionSystem
{
    /// <summary>
    /// Logica di interazione per DataWindow.xaml
    /// </summary>
    public partial class DataWindow : Window
    {
        private const string PIPENAME = "pds_detection_system";

        protected Process TCPServer;
        protected MySqlConnection DBconnection;
        protected NamedPipeServerStream ServerPipe;

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

            /*** Pipe handle Function ***/
            Thread thread = new Thread(new ThreadStart(PipeSyncFunction));
            thread.Start();
            
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

            /*** BASIC COLUMN CHART ***/
            SeriesCollection = new SeriesCollection
            {
                new ColumnSeries
                {
                    Title = "2015",
                    Values = new ChartValues<double> { 10, 50, 39, 50 }
                }
            };

            //adding series will update and animate the chart automatically
            SeriesCollection.Add(new ColumnSeries
            {
                Title = "2016",
                Values = new ChartValues<double> { 11, 56, 42 }
            });

            //also adding values updates and animates the chart automatically
            SeriesCollection[1].Values.Add(48d);

            Labels = new[] { "Maria", "Susan", "Charles", "Frida" };
            Formatter = value => value.ToString("N");

            DataContext = this;

            /*** SCATTER PLOT CHART ***/
            var rand = new Random();
            ValuesA = new ChartValues<ObservablePoint>();
            ValuesB = new ChartValues<ObservablePoint>();
            

            for (var i = 0; i < 20; i++)
            {
                ValuesA.Add(new ObservablePoint(rand.NextDouble() * 10, rand.NextDouble() * 10));
                ValuesB.Add(new ObservablePoint(rand.NextDouble() * 10, rand.NextDouble() * 10));
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
        

        public SeriesCollection SeriesCollection { get; set; }
        public string[] Labels { get; set; }
        public Func<double, string> Formatter { get; set; }

        /*** SCATTER PLOT ***/
        public ChartValues<ObservablePoint> ValuesA { get; set; }
        public ChartValues<ObservablePoint> ValuesB { get; set; }
        public ChartValues<ObservablePoint> ValuesC { get; set; }


        public void PipeSyncFunction() {
            try{
                while (true)
                {
                    using (var ServerPipe = new NamedPipeServerStream(PIPENAME))
                    {
                        ServerPipe.WaitForConnection();
                        StreamReader reader = new StreamReader(ServerPipe);
                        //WriteOnTextBox("Reading from pipe: ");
                        while (reader.Peek() != -1)
                        {
                            WriteOnTextBox((char)reader.Read() + "");
                        }

                        if (ServerPipe.IsConnected)
                        {
                            // must disconnect 
                            ServerPipe.Disconnect();
                        }
                    }
                }
            }
            catch (Exception exception)
            {
                WriteOnTextBox(exception.Message);
            }
        }

        public void WriteOnTextBox(string message) {
            Application.Current.Dispatcher.Invoke(() => {
                output_box.AppendText(message);
            });
        }

        private void GoToMapClick(object sender, RoutedEventArgs e)
        {
            MapWindow mapWin = new MapWindow();
            mapWin.Show();
        }
    }
}
