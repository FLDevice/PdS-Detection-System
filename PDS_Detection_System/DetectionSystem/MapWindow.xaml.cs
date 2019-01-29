using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Shapes;
using MySql.Data.MySqlClient;

namespace DetectionSystem
{
    /// <summary>
    /// Interaction logic for Window1.xaml
    /// </summary>
    public partial class MapWindow : Window
    {
        protected MySqlConnection DBconnection;

        private int maxX, minX, maxY, minY, espCount;

        private const int ellipseSize = 10;

        private long previouslyChecked = 0;

        private List<ESP> eSPs;
        
        // Information needed for the animation
        private long unixBegin = 0, unixLast = 0, granularity = 100;

        public MapWindow()
        {
            InitializeComponent();

            // We need to determine what is the scale of the map.
            try
            {
                DBconnection = new MySqlConnection();
                DBconnection.ConnectionString = "server=localhost; database=pds_db; uid=pds_user; pwd=password";
                DBconnection.Open();

                MySqlCommand cmm = new MySqlCommand(
                    "SELECT MAX(x), MIN(x), MAX(y), MIN(y), COUNT(*) FROM ESP", DBconnection);
                MySqlDataReader dataReader = cmm.ExecuteReader();

                while (dataReader.Read())
                {
                    maxX = dataReader.GetInt32(0);
                    minX = dataReader.GetInt32(1);
                    maxY = dataReader.GetInt32(2);
                    minY = dataReader.GetInt32(3);
                    espCount = dataReader.GetInt32(4);
                }

                //close Data Reader
                dataReader.Close();

                cmm = new MySqlCommand(
                    "SELECT id, mac, x, y FROM ESP", DBconnection);
                dataReader = cmm.ExecuteReader();

                eSPs = new List<ESP>();
                
                while (dataReader.Read())
                {
                     eSPs.Add(new ESP(dataReader.GetInt32(0), dataReader.GetString(1), dataReader.GetInt32(2), dataReader.GetInt32(3)));
                }

                //close Data Reader
                dataReader.Close();

                DBconnection.Close();
            }
            catch (Exception exc)
            {
                System.Windows.MessageBox.Show("The following error occurred: \n\n" + exc.Message);
                DBconnection.Close();
                this.Close();
            }
        }

        private void Update_Click(object sender, RoutedEventArgs e)
        {
            ClearMap();

            string selectQuery;

            selectQuery = "SELECT d.mac, d.x, d.y, d.timestamp "
                        + "FROM devices d "
                        + "JOIN( "
                        + "SELECT mac, MAX(timestamp) timestamp, MAX(dev_id) as max_id "
                        + "FROM devices "
                        + "GROUP BY mac "
                        + ") x ON(x.max_id = d.dev_id) "
                        + "WHERE UNIX_TIMESTAMP(d.timestamp) > UNIX_TIMESTAMP(NOW()) - 120 ";

            if(CheckMacAddress(macBox.Text))
                selectQuery += "AND d.mac = '" + macBox.Text + "'";
            
            try
            {
                DBconnection = new MySqlConnection();
                DBconnection.ConnectionString = "server=localhost; database=pds_db; uid=pds_user; pwd=password";
                DBconnection.Open();

                try
                {
                    MySqlCommand cmm = new MySqlCommand(selectQuery, DBconnection);
                    MySqlDataReader dataReader = cmm.ExecuteReader();

                    while (dataReader.Read())
                    {
                        DrawDevice(dataReader.GetString(0), dataReader.GetDouble(1), dataReader.GetDouble(2));
                    }
                    
                    //close Data Reader
                    dataReader.Close();
                }
                catch (Exception exc)
                {
                    System.Windows.MessageBox.Show("The following error occurred: \n\n" + exc.Message);
                }

                DBconnection.Close();
            }
            catch (Exception exc)
            {
                System.Windows.MessageBox.Show("The following error occurred: \n\n" + exc.Message);
                DBconnection.Close();
                this.Close();
            }
        }

        // Check the correctness of the given mac address
        private bool CheckMacAddress(string mac)
        {
            // Define a regular expression for repeated words.
            Regex rx = new Regex(@"^(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}|(?:[0-9a-fA-F]{2}-){5}[0-9a-fA-F]{2}|(?:[0-9a-fA-F]{2}){5}[0-9a-fA-F]{2}$");

            return rx.IsMatch(mac);
        }

        private void Animation_Click(object sender, RoutedEventArgs e)
        {
            if (beginDateTime.Text == null || lastDateTime.Text == null || granularityBox.Text == null)
                return;

            ClearMap();

            DateTime begin = Convert.ToDateTime(beginDateTime.Text);
            DateTimeOffset beginOffset = new DateTimeOffset(begin);
            unixBegin = beginOffset.ToUnixTimeSeconds();
            DateTime last = Convert.ToDateTime(lastDateTime.Text);
            DateTimeOffset lastOffset = new DateTimeOffset(last);
            unixLast = lastOffset.ToUnixTimeSeconds();
            granularity = Convert.ToInt64(granularityBox.Text);

            if (unixBegin >= unixLast)
                return;

            try
            {
                DBconnection = new MySqlConnection();
                DBconnection.ConnectionString = "server=localhost; database=pds_db; uid=pds_user; pwd=password";
                DBconnection.Open();

                for (long currentTs = unixBegin; currentTs <= unixLast; currentTs += granularity)
                {
                    int i = 1;

                    try
                    {
                        MySqlCommand cmm = new MySqlCommand(
                            "SELECT d.mac, d.x, d.y, d.timestamp "
                            + "FROM devices d "
                            + "JOIN( "
                            + "SELECT mac, MAX(timestamp) timestamp, MAX(dev_id) as max_id "
                            + "FROM devices "
                            + "GROUP BY mac "
                            + ") x ON(x.max_id = d.dev_id) "
                            + "WHERE UNIX_TIMESTAMP(d.timestamp) > " + Convert.ToString(currentTs) + " - 120 " 
                            + "AND UNIX_TIMESTAMP(d.timestamp) <= " + Convert.ToString(currentTs), DBconnection);
                        MySqlDataReader dataReader = cmm.ExecuteReader();

                        while (dataReader.Read())
                        {
                            DrawDevice("device" + i + "_" + Convert.ToString(currentTs), dataReader.GetString(0), dataReader.GetDouble(1), dataReader.GetDouble(2));
                            i++;
                        }

                        //close Data Reader
                        dataReader.Close();
                    }
                    catch (Exception exc)
                    {
                        System.Windows.MessageBox.Show("The following error occurred: \n\n" + exc.Message);
                    }
                }

                DBconnection.Close();
            }
            catch (Exception exc)
            {
                System.Windows.MessageBox.Show("The following error occurred: \n\n" + exc.Message);
                DBconnection.Close();
                this.Close();
            }

            SetUpSlider();
            UpdateAnimation(unixBegin);
        }

        private void ClearMap()
        {
            mapOfDevices.Children.Clear();
            for (long currentTs = unixBegin; currentTs <= unixLast; currentTs += granularity)
            {
                Console.WriteLine("CurrentTS " + currentTs);
                int i = 0;
                bool exit = false;
                string name;

                while (!exit)
                {
                    name = "device" + i + "_" + currentTs;

                    if (mapOfDevices.FindName(name) != null)
                        mapOfDevices.UnregisterName(name);
                    else
                        exit = true;
                }
            }

            foreach (ESP curEsp in eSPs)
            {
                DrawESP(curEsp);
            }

            timeSlider.Visibility = Visibility.Hidden;
            previouslyChecked = 0;
            unixBegin = 0;
            unixLast = 0;
            granularity = 100;
        }

        private void DrawDevice(string deviceName, double left, double bottom)
        {
            // Create a red Ellipse.
            Ellipse myEllipse = new Ellipse();

            // Create a SolidColorBrush with a red color to fill the 
            // Ellipse with.
            SolidColorBrush mySolidColorBrush = new SolidColorBrush();

            // Describes the brush's color using RGB values. 
            // Each value has a range of 0-255.
            mySolidColorBrush.Color = Color.FromArgb(255, 255, 255, 0);
            myEllipse.Fill = mySolidColorBrush;
            myEllipse.StrokeThickness = 2;
            myEllipse.Stroke = Brushes.Black;

            // Set the width and height of the Ellipse.
            myEllipse.Width = ellipseSize;
            myEllipse.Height = ellipseSize;

            // Add a ToolTip that shows the name of the Device and its position 
            ToolTip tt = new ToolTip();
            tt.Content = "MAC: " + deviceName + "\nX: " + left + "- Y: " + bottom;
            myEllipse.ToolTip = tt;

            // Add the Ellipse to the Canvas.
            mapOfDevices.Children.Add(myEllipse);
            
            Canvas.SetLeft(myEllipse, ScaleX(left));
            Canvas.SetBottom(myEllipse, ScaleY(bottom));
        }

        private void DrawESP(ESP curEsp)
        {
            // Create a red Ellipse.
            Ellipse myEllipse = new Ellipse();

            // Create a SolidColorBrush with a red color to fill the 
            // Ellipse with.
            SolidColorBrush mySolidColorBrush = new SolidColorBrush();

            // Describes the brush's color using RGB values. 
            // Each value has a range of 0-255.
            mySolidColorBrush.Color = Color.FromArgb(255, 66, 119, 244);
            myEllipse.Fill = mySolidColorBrush;
            myEllipse.StrokeThickness = 2;
            myEllipse.Stroke = Brushes.Black;

            // Set the width and height of the Ellipse.
            myEllipse.Width = ellipseSize;
            myEllipse.Height = ellipseSize;

            // Add a ToolTip that shows the name of the Device and its position 
            ToolTip tt = new ToolTip();
            tt.Content = "ESP " + curEsp.Id + "\nMAC: " + curEsp.Mac + "\nX: " + curEsp.X + "- Y: " + curEsp.Y;
            myEllipse.ToolTip = tt;

            // Add the Ellipse to the Canvas.
            mapOfDevices.Children.Add(myEllipse);

            Canvas.SetLeft(myEllipse, ScaleX(curEsp.X));
            Canvas.SetBottom(myEllipse, ScaleY(curEsp.Y));
        }

        private void DrawDevice(string name, string deviceName, double left, double bottom)
        {
            // Create a red Ellipse.
            Ellipse myEllipse = new Ellipse();

            // Set the name
            myEllipse.Name = name;

            // Create a SolidColorBrush with a red color to fill the 
            // Ellipse with.
            SolidColorBrush mySolidColorBrush = new SolidColorBrush();

            // Describes the brush's color using RGB values. 
            // Each value has a range of 0-255.
            mySolidColorBrush.Color = Color.FromArgb(255, 255, 255, 0);
            myEllipse.Fill = mySolidColorBrush;
            myEllipse.StrokeThickness = 2;
            myEllipse.Stroke = Brushes.Black;

            // Set the width and height of the Ellipse.
            myEllipse.Width = ellipseSize;
            myEllipse.Height = ellipseSize;

            // Add a ToolTip that shows the name of the Device and its position 
            ToolTip tt = new ToolTip();
            tt.Content = "MAC: " + deviceName + "\nX: " + left + "- Y: " + bottom;
            myEllipse.ToolTip = tt;

            // Set the visibility to hidden
            myEllipse.Visibility = Visibility.Hidden;

            if (mapOfDevices.FindName(name) != null)
                mapOfDevices.UnregisterName(name);
            mapOfDevices.RegisterName(name, myEllipse);

            // Add the Ellipse to the Canvas.
            mapOfDevices.Children.Add(myEllipse);

            Canvas.SetLeft(myEllipse, ScaleX(left));
            Canvas.SetBottom(myEllipse, ScaleY(bottom));
        }

        private double ScaleX(double value)
        {
            double scaledValue;
            int width = Convert.ToInt32(mapOfDevices.Width);

            if (maxX - minX != 0)
                scaledValue = (value * width / (maxX - minX)) - ellipseSize / 2;
            else
                scaledValue = width / 2;

            return scaledValue;
        }

        private double ScaleY(double value)
        {
            double scaledValue;
            int height = Convert.ToInt32(mapOfDevices.Height);

            if (maxY - minY != 0)
                scaledValue = (value * height / (maxY - minY)) - ellipseSize / 2;
            else
                scaledValue = height / 2;

            return scaledValue;
        }


        private void Slider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            UpdateAnimation(Convert.ToInt64(timeSlider.Value));
        }

        private void SetUpSlider()
        {
            timeSlider.Minimum = unixBegin;
            timeSlider.Maximum = unixLast;
            timeSlider.SmallChange = granularity;
            timeSlider.LargeChange = granularity*5;
            timeSlider.TickFrequency = granularity;
            timeSlider.Value = unixBegin;
            timeSlider.Visibility = Visibility.Visible;
        }
        
        private void UpdateAnimation(long timestamp)
        {
            int i = 1;
            bool exit = false;
            
            if (previouslyChecked != 0)
            {
                while (!exit)
                {
                    Ellipse device = (Ellipse)mapOfDevices.FindName("device" + i + "_" + previouslyChecked);

                    if (device == null)
                    {
                        exit = true;
                    }
                    else
                    {
                        device.Visibility = Visibility.Hidden;
                        i++;
                    }
                }
            }

            i = 1;
            exit = false;

            while (!exit)
            {
                Ellipse device = (Ellipse)mapOfDevices.FindName("device" + i + "_" + timestamp);

                if (device == null)
                {
                    exit = true;
                }
                else
                {
                    device.Visibility = Visibility.Visible;

                    i++;
                }
            }

            previouslyChecked = timestamp;
        }
    }

    public class ESP
    {
        private int x, y, id;
        public int Id
        {
            get { return id; }
        }
        public int X
        {
            get { return x; }
        }
        public int Y
        {
            get { return y; }
        }
        private string mac;
        public string Mac
        {
            get { return mac; }
        }

        public ESP(int id, string mac, int x, int y)
        {
            this.id = id;
            this.mac = mac;
            this.x = x;
            this.y = y;
        }
    }
}
