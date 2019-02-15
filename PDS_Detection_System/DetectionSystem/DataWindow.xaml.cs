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
        Label macLabel;
        Label devicesLabel;
        Label riskLabel;

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
            macLabel = (Label)this.FindName("mac_label");
            devicesLabel = (Label)this.FindName("device_label");
            riskLabel = (Label)this.FindName("risk_label");
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


            /*** BASIC LINE CHART ***/
            SeriesCollection = new SeriesCollection
            {
                new LineSeries
                {
                    Title = "Devices number",
                    Values = new ChartValues<double> {}
                }
            };

            /*** BASIC COLUMN CHART ***/
            ColumnCollection = new SeriesCollection {};
            
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
            long granularity = Convert.ToInt64(GranularityPickerStack.Text);
            int devices_num = Convert.ToInt32(DevNumPickerCol.Text);

            MySqlCommand cmm = null;
            try
            {
                // Initialize the graph structure and clear existing data
                cmm = new MySqlCommand("SELECT mac FROM devices"
                                    + " WHERE timestamp BETWEEN '" + timestart + "' AND '" + timestop + "'"
                                    + " GROUP BY mac"
                                    + " ORDER BY count(*) DESC LIMIT " + devices_num, DBconnection);


                MySqlDataReader r = cmm.ExecuteReader();

                List<string> labs = new List<string>();
                Dictionary<string, int> map = new Dictionary<string, int>();

                // Clear old data
                if (ColumnCollection.Count!=0)
                    ColumnCollection.Clear(); 

                // For each mac read, store the address in the map and add a series to the chart
                int count = 0;
                while (r.Read())
                {
                    if (!map.ContainsKey(r[0].ToString()))
                    {
                        map.Add(r[0].ToString(), count);
                        count++;

                        ColumnCollection.Add(new StackedColumnSeries
                        {
                            Title = r[0].ToString(),
                            Values = new ChartValues<double> { },
                            StackMode = StackMode.Values
                        });
                    }
                }

                r.Close();

                // Now that the graph is correctly initialized, read the actual data about the most frequent devices
                cmm = new MySqlCommand("SELECT (unix_timestamp(timestamp) - unix_timestamp(timestamp)%" + granularity + ") groupTime, d.mac, count(*) "
                                        + " FROM devices d JOIN(SELECT mac FROM devices"
                                        +                     " WHERE timestamp BETWEEN '" + timestart + "' AND '" + timestop + "'"
                                        +                     " GROUP BY mac"
                                        +                     " ORDER BY count(*) DESC LIMIT " + devices_num 
                                        + ") x ON(x.mac = d.mac)"
                                        + " WHERE timestamp BETWEEN '" + timestart + "' AND '" + timestop + "'"
                                        + " GROUP BY groupTime, d.mac", DBconnection);


                r = cmm.ExecuteReader();
                
                bool first = true;
                int actualDevicesNum = map.Count;
                Ts_structure TS = new Ts_structure(0, actualDevicesNum);
                while (r.Read())
                {
                    // The first time we iterate through the loop we need to properly initialize the ts_structure 
                    if (first) 
                    {
                        TS = new Ts_structure(Convert.ToInt64(r[0]), actualDevicesNum);
                        first = false;
                    }

                    // If the timestamp is different from the previous one, it means we completed the analisys of the previous time segment, 
                    // hence its time to update the graph with the macs collected during that timeframe.
                    if (TS.Timestamp != Convert.ToInt64(r[0]))
                    {
                        // Even if some MAC wasn't read during a specific time segment, we still need to keep track of its absence in the graph
                        if (TS.MACMap.Count != actualDevicesNum) {
                            TS.AddMAC(map, 0);     
                        }

                        // Add the data to the graph
                        foreach (KeyValuePair<string, int> mac in TS.MACMap) {
                            ColumnCollection[map[mac.Key]].Values.Add(Convert.ToDouble(mac.Value));
                        }
                        DateTime dIn = TimeStampToDateTime(TS.Timestamp);
                        labs.Add(dIn.ToShortDateString() + "\n  " + dIn.ToString("HH:mm:ss"));

                        TS = new Ts_structure(Convert.ToInt64(r[0]), actualDevicesNum);
                    }

                    // Add the mac address and its occurrence within the time segment in the TS_structure
                    TS.AddMAC(r[1].ToString(), Convert.ToInt32(r[2]));
                    
                }

                // When we go out of the r.Read() while loop, we still need to add the last data to the graph.
                // If any data was read from the database, then finish the graph update with the last data.
                if(!first)
                {                           
                    // Even if some MAC wasn't read during a specific time segment, we still need to keep track of its absence in the graph
                    if (TS.MACMap.Count != actualDevicesNum)
                    {
                        TS.AddMAC(map, 0);
                    }

                    // Add the data to the graph
                    foreach (KeyValuePair<string, int> mac in TS.MACMap)
                    {
                        ColumnCollection[map[mac.Key]].Values.Add(Convert.ToDouble(mac.Value));
                    }
                    DateTime d = TimeStampToDateTime(TS.Timestamp);
                    labs.Add(d.ToShortDateString() + "\n  " + d.ToString("HH:mm:ss"));
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

        public class Ts_structure
        {
            private int mac_num;
            private long timestamp;
            Dictionary<string, int> mac_map;

            public long Timestamp {
                get { return timestamp; }
            }

            public void AddMAC(string mac, int value) {
                if (!mac_map.ContainsKey(mac))
                {
                    mac_map.Add(mac, value);
                }
            }

            public void AddMAC(Dictionary<string, int> map, int value)
            {
                foreach (KeyValuePair<string, int> item in map)
                {
                    AddMAC(item.Key, value);
                }
            }

            public Dictionary<string, int> MACMap {
                get { return mac_map; }
            }

            public Ts_structure(long timestamp, int mac_num)
            {
                this.timestamp = timestamp;
                this.mac_num = mac_num;
                mac_map = new Dictionary<string, int>();
            }
        }

        private void Update_chart_err_Click(object sender, RoutedEventArgs e)
        { 

            string timestart = StartTimePickereErr.Text;
            string timestop = StopTimePickerErr.Text;
            List<LocalAddress> AddrList = new List<LocalAddress>();
            List<double> errors = new List<double>();

            int total_address = 0;
            int devices_count = 0;
            double err_seq_n = 0;
            double err_dist = 0;
            double local_error_avg = 0;
            double error_avg = 0;

            MySqlCommand cmm = null;

            try
            {
                cmm = new MySqlCommand("SELECT count(distinct(mac)) FROM local_macs " +
                                       "WHERE timestamp BETWEEN '" + timestart + "' AND '" + timestop + "'",
                                       DBconnection);
                MySqlDataReader r = cmm.ExecuteReader();
                while (r.Read())
                {
                    total_address = Convert.ToInt32(r[0]);
                }
                cmm.Dispose();
           
                // Initialize the graph structure and clear existing data
                cmm = new MySqlCommand("SELECT mac, x, y, m.timestamp, seq_ctl from local_macs AS m " +
                                        "JOIN (local_packets AS p) ON (m.mac = p.addr) " +
                                        "WHERE m.timestamp BETWEEN '" + timestart + "' AND '" + timestop + "'"+
                                        "GROUP BY m.timestamp, seq_ctl " +
                                        "ORDER BY m.timestamp", DBconnection);
                MySqlDataReader r1 = cmm.ExecuteReader();

                while (r1.Read())
                {
                    LocalAddress lA = new LocalAddress(Convert.ToString(r1[0]), 
                                                       int.Parse(Convert.ToString(r1[4]), System.Globalization.NumberStyles.HexNumber),
                                                       Convert.ToInt64(Convert.ToDateTime(r1[3]).Ticks),
                                                       Convert.ToDouble(r1[1]), 
                                                       Convert.ToDouble(r1[2])
                                        );
                    AddrList.Add(lA);
                }
                cmm.Dispose();
            }
            catch (Exception ex)
            {
                if (cmm != null)
                    cmm.Dispose();
                output_box.AppendText("" + ex.Message);
                Console.WriteLine(ex.StackTrace);
            }


            // Versione semplice senza l'associazione del numero di mac ad ogni device
            for (int i = 0; i < AddrList.Count; i++) {
                LocalAddress lA = AddrList.ElementAt<LocalAddress>(i);

                TimeSpan lASpan = new TimeSpan(lA.timestamp);
                double lA_seconds = lASpan.TotalSeconds;

                if (!lA.isChecked) {

                    lA.isChecked = true;
                    devices_count++;

                    for (int j = i+1; j < AddrList.Count; j++) {
                        LocalAddress lAin = AddrList.ElementAt<LocalAddress>(j);
                        if (lAin.mac.Equals(lA.mac)) {
                            lAin.isChecked = true;
                            continue;
                        }
                        TimeSpan lAinSpan = new TimeSpan(lAin.timestamp);
                        double lAin_seconds = lAinSpan.TotalSeconds;

                        if (!lAin.isChecked) {
                            double timeDiff = lAin_seconds - lA_seconds;
                            int k = 5; //Multiply factor(meter)
                            if ((lAin.seq_n > lA.seq_n) && (lAin.seq_n - lA.seq_n < 1280)) { // 1280 = 0x500
                                err_seq_n = (double)(lAin.seq_n - lA.seq_n) / 1280;
                                if (timeDiff >= 0) {
                                    if (lA.getDistance(lAin) < k * timeDiff) {
                                        err_dist = (double)lA.getDistance(lAin) / (k * timeDiff);

                                        local_error_avg = (double)(err_seq_n + err_dist) / 2;
                                        errors.Add(local_error_avg);
                                        
                                        lAin.isChecked = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }


            double tot = 0;
            foreach (double error in errors) {
                tot = tot + error;
            }
            error_avg = (double)tot / errors.Count;

            string mac_num_msg = "Local MAC addresses detected:\t " + total_address + "\n";
            string disp_num_msg = "Real devices estimation:\t " + devices_count + "\n";
            string risk_msg = "";
            if (errors.Count == 0)
                risk_msg = "Estimation risk:\t " + 0 + " %\n";
            else
                risk_msg = "Estimation risk:\t " + (error_avg * 100).ToString("F1") + " %\n";

            macLabel.Content = mac_num_msg;
            devicesLabel.Content = disp_num_msg;
            riskLabel.Content = risk_msg;
        }

        public class LocalAddress {
            public string mac;
            public int seq_n;
            public long timestamp;
            public double x;
            public double y;
            public bool isChecked;

            public LocalAddress(string mac, int seq_n, long timestamp, double x, double y) {
                this.mac = mac;
                this.seq_n = seq_n;
                this.timestamp = timestamp;
                this.x = x;
                this.y = y;
                this.isChecked = false;
            }
            
            public double getDistance(LocalAddress p2){
                return Math.Sqrt(Math.Pow((this.x - p2.x), 2) + Math.Pow((this.y - p2.y), 2));
            }

        }


    }
}
