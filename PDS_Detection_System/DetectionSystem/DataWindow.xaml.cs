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
using System.ComponentModel;

namespace DetectionSystem
{
    /// <summary>
    /// Logica di interazione per DataWindow.xaml
    /// </summary>
    public partial class DataWindow : Window, INotifyPropertyChanged
    {
        private const string PIPENAME = "pds_detection_system";

        protected Process TCPServer;
        protected MySqlConnection DBconnection;
        protected NamedPipeServerStream ServerPipe;

        private string fileN;
        private string args;

        public static TextBox output_box;
        private string[] _LabelsDev;
        private string[] _ColumnLabels;
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
            ServerPipe = new NamedPipeServerStream(PIPENAME, PipeDirection.InOut, NamedPipeServerStream.MaxAllowedServerInstances, PipeTransmissionMode.Message, PipeOptions.Asynchronous);
            ServerPipe.BeginWaitForConnection(new AsyncCallback(PipeASyncFunction), this);
            
            /*
             * 1. Crea NamedPipeServerStream qui
             * 2. Chiama BeginWaitForConnection(AsyncCallback, Object)
             * 3. Alla chiusura della finestra chiudi la Pipe
             * 
             */

            MySqlCommand cmm = null;
            try
            {
                DBconnection = new MySqlConnection();
                DBconnection.ConnectionString = "server=localhost; database=pds_db; uid=pds_user; pwd=password";
                DBconnection.Open();
                cmm = new MySqlCommand("select count(*) from devices", DBconnection);
                MySqlDataReader r = cmm.ExecuteReader();
                while (r.Read())
                {
                    output_box.AppendText("" + r[0]);
                }
                cmm.Dispose();
            }
            catch (Exception e)
            {
                output_box.AppendText(e.Message + "\n");
                output_box.ScrollToEnd();
                cmm.Dispose();
            }
       

            /*** BASIC COLUMN CHART ***/
            SeriesCollection = new SeriesCollection
            {
                new LineSeries
                {
                    Title = "Devices number",
                    Values = new ChartValues<double> {}
                }
            };

            ColumnCollection = new SeriesCollection
            {
                new ColumnSeries
                {
                    Title = "MAC's frequencies",
                    Values = new ChartValues<double> {}
                }
            };

            /*** SCATTER PLOT CHART ***/
            /*
            var rand = new Random();
            ValuesA = new ChartValues<ObservablePoint>();
            ValuesB = new ChartValues<ObservablePoint>();


            for (var i = 0; i < 20; i++)
            {
                ValuesA.Add(new ObservablePoint(rand.NextDouble() * 10, rand.NextDouble() * 10));
                ValuesB.Add(new ObservablePoint(rand.NextDouble() * 10, rand.NextDouble() * 10));
            }
            */
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
            //pipe_thread_stop = true;
            if (is_running)
            {
                // if already terminated do nothing
                if (TCPServer.ProcessName.Length == 0) return;

                TCPServer.Kill();
                TCPServer.WaitForExit();
            }
            DBconnection.Close();
            if(ServerPipe.IsConnected)
                ServerPipe.Close();
        }
        
        /*
         Line chart
        */
        public SeriesCollection SeriesCollection { get; set; }
        public string[] LabelsDev {
            get { return _LabelsDev; }
            set
            {
                _LabelsDev = value;
                OnPropertyChanged("LabelsDev");
            }
        }
        public Func<double, string> Formatter { get; set; }

        /*
         Column chart
        */
        public SeriesCollection ColumnCollection { get; set; }
        public string[] ColumnLabels{
            get { return _ColumnLabels; }
            set
            {
                _ColumnLabels = value;
                OnPropertyChanged("ColumnLabels");
            }
        }
        public Func<double, string> ColumnFormatter { get; set; }

        /*** SCATTER PLOT ***/
        public ChartValues<ObservablePoint> ValuesA { get; set; }
        public ChartValues<ObservablePoint> ValuesB { get; set; }
        public ChartValues<ObservablePoint> ValuesC { get; set; }


        private void PipeASyncFunction(IAsyncResult result) {
            try
            {
                ServerPipe.EndWaitForConnection(result);
                StreamReader reader = new StreamReader(ServerPipe);

                //WriteOnTextBox("Reading from pipe: ");
                while (reader.Peek() != -1)
                {
                    WriteOnTextBox((char)reader.Read() + "");
                    //TODO Switch messaggi server
                }

                // Kill original pipe and create new wait pipe  
                ServerPipe.Close();
                ServerPipe = null;
                
                // Recursively wait for the connection again and again....
                ServerPipe = new NamedPipeServerStream(PIPENAME, PipeDirection.InOut, NamedPipeServerStream.MaxAllowedServerInstances, PipeTransmissionMode.Message, PipeOptions.Asynchronous);
                ServerPipe.BeginWaitForConnection(new AsyncCallback(PipeASyncFunction), this);
            }
            catch(Exception e )
            {
                Console.WriteLine(e.StackTrace);
                return;
            }
            
        }

        public void WriteOnTextBox(string message) {
            Application.Current.Dispatcher.Invoke(() => {
                output_box.AppendText(message);
            });
        }


        private void Update_chart_Click(object sender, RoutedEventArgs e)
        {

            string timestart = StartTimePicker.Text;
            string timestop = StopTimePicker.Text;
            long granularity = Convert.ToInt64(GranularityPicker.Text);

            MySqlCommand cmm = null;
            try
            {
                /*
                cmm = new MySqlCommand("SELECT COUNT(DISTINCT mac) FROM devices WHERE timestamp BETWEEN '"
                                                    +timestart+"' AND '"+timestop+"'", DBconnection);

               */

                cmm = new MySqlCommand("SELECT (unix_timestamp(timestamp) - unix_timestamp(timestamp)%" + granularity + ") groupTime, count(*)"
                                                    + " FROM devices WHERE timestamp BETWEEN '" + timestart + "' AND '" + timestop + "'"
                                                    + " GROUP BY groupTime", DBconnection);
                MySqlDataReader r = cmm.ExecuteReader();

                // Create structure and clear data
                List<string> labs = new List<string>();
                SeriesCollection[0].Values.Clear();
                // Read content of SQL command execution and add data to the graph
                int n_counter = 0;
                long previous_timestamp = 0;
                while (r.Read())
                {
                    if (n_counter == 0)
                    {
                        previous_timestamp = Convert.ToInt64(r[0]);
                        SeriesCollection[0].Values.Add(Convert.ToDouble(r[1]));
                        DateTime date = TimeStampToDateTime(Convert.ToInt64(r[0]));
                        labs.Add(date.ToShortDateString() + "\n  " + date.ToString("HH:mm:ss"));
                    }
                    else
                    {
                        if ((Convert.ToInt64(r[0]) - previous_timestamp) != granularity)
                        {
                            for (long i = granularity; i < (Convert.ToInt64(r[0]) - previous_timestamp); i += granularity)
                            {
                                SeriesCollection[0].Values.Add(Convert.ToDouble(0));
                                DateTime d = TimeStampToDateTime(previous_timestamp + i);
                                labs.Add(d.ToShortDateString() + "\n  " + d.ToString("HH:mm:ss"));
                            }
                        }
                        SeriesCollection[0].Values.Add(Convert.ToDouble(r[1]));
                        DateTime date = TimeStampToDateTime(Convert.ToInt64(r[0]));
                        labs.Add(date.ToShortDateString() + "\n  " + date.ToString("HH:mm:ss"));
                        previous_timestamp = Convert.ToInt64(r[0]);
                    }
                    n_counter++;
                }
                //Prepare labels
                string[] ls = new string[labs.Count];
                for (int i = 0; i < labs.Count; i++)
                {
                    ls[i] = labs[i];
                }
                LabelsDev = ls;
                Formatter = value => value.ToString();
                //Send data to the graph
                DataContext = this;
                cmm.Dispose();

            }
            catch (Exception ex)
            {
                if (cmm != null)
                    cmm.Dispose();
                output_box.AppendText("" + ex.Message);
                Console.WriteLine(ex.StackTrace);
            }
        }

        private void GoToMapClick(object sender, RoutedEventArgs e)
        {
            MapWindow mapWin = new MapWindow();
            mapWin.Show();
        }
        
        /*
         * Handle the modification to the data of the graph and send them to it 
         */
        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName = null)
        {
            if (PropertyChanged != null)
                PropertyChanged.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }

                 
        public static DateTime TimeStampToDateTime(double unixTimeStamp)
        {
            // Unix timestamp is seconds past epoch
            System.DateTime dtDateTime = new DateTime(1970, 1, 1, 0, 0, 0, 0, System.DateTimeKind.Utc);
            dtDateTime = dtDateTime.AddSeconds(unixTimeStamp).ToLocalTime();
            return dtDateTime;
        }

        private void Update_chart_col_Click(object sender, RoutedEventArgs e){
            string timestart = StartTimePickerCol.Text;
            string timestop = StopTimePickerCol.Text;
            long devices_num = Convert.ToInt64(DevNumPickerCol.Text);

            MySqlCommand cmm = null;
            try
            {
                
                cmm = new MySqlCommand("SELECT mac, count(*)"
                                        + " FROM devices WHERE timestamp BETWEEN '" + timestart + "' AND '" + timestop + "'"
                                        + " GROUP BY mac" 
                                        + " ORDER BY count(*) DESC", DBconnection);
                MySqlDataReader r = cmm.ExecuteReader();

                // Create structure and clear data
                List<string> labs = new List<string>();
                ColumnCollection[0].Values.Clear();
                long count = 1;
                while (r.Read())
                {
                    ColumnCollection[0].Values.Add(Convert.ToDouble(r[1]));
                    labs.Add(r[0].ToString());
                    if (count == devices_num)
                        break;
                    count++;
                }
                //Prepare labels
                string[] ls = new string[labs.Count];
                for (int i = 0; i < labs.Count; i++)
                {
                    ls[i] = labs[i];
                }
                ColumnLabels = ls;
                ColumnFormatter = value => value.ToString();
                //Send data to the graph
                DataContext = this;
                cmm.Dispose();
            }
            catch (Exception ex)
            {
                if (cmm != null)
                    cmm.Dispose();
                output_box.AppendText("" + ex.Message);
                Console.WriteLine(ex.StackTrace);
            }
        }
    }
}
