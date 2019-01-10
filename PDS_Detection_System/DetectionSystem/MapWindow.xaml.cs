using System;
using System.Collections.Generic;
using System.ComponentModel; // CancelEventArgs
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;
using MySql.Data.MySqlClient;
using Xceed.Wpf.Toolkit;

namespace DetectionSystem
{
    /// <summary>
    /// Interaction logic for Window1.xaml
    /// </summary>
    public partial class MapWindow : Window
    {
        protected MySqlConnection DBconnection;

        private int maxX, minX, maxY, minY;

        private const int ellipseSize = 10;

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
                    "SELECT MAX(x), MIN(x), MAX(y), MIN(y) FROM ESP", DBconnection);
                MySqlDataReader dataReader = cmm.ExecuteReader();

                while (dataReader.Read())
                {
                    maxX = dataReader.GetInt32(0);
                    minX = dataReader.GetInt32(1);
                    maxY = dataReader.GetInt32(2);
                    minY = dataReader.GetInt32(3); 
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
            mapOfESP.Children.Clear();

            string selectQuery;

            selectQuery = "SELECT d.mac, d.x, d.y, d.timestamp "
                        + "FROM devices d "
                        + "JOIN( "
                        + "SELECT mac, MAX(timestamp) timestamp "
                        + "FROM devices "
                        + "GROUP BY mac "
                        + ") x ON(x.mac = d.mac AND x.timestamp = d.timestamp) "
                        + "WHERE d.timestamp > UNIX_TIMESTAMP(NOW()) - 120 ";

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
                        DrawDevice(dataReader.GetString(0), dataReader.GetInt32(1), dataReader.GetInt32(2));
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

        private void DrawDevice(string deviceName, int left, int bottom)
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
            mapOfESP.Children.Add(myEllipse);
            
            Canvas.SetLeft(myEllipse, scaleX(left));
            Canvas.SetBottom(myEllipse, scaleY(bottom));
        }

        private int scaleX(int value)
        {
            int scaledValue;
            int width = Convert.ToInt32(mapOfESP.Width);

            if (maxX - minX != 0)
                scaledValue = (value * width / (maxX - minX)) - ellipseSize / 2;
            else
                scaledValue = width / 2;

            return scaledValue;
        }

        private int scaleY(int value)
        {
            int scaledValue;
            int height = Convert.ToInt32(mapOfESP.Height);

            if (maxY - minY != 0)
                scaledValue = (value * height / (maxY - minY)) - ellipseSize / 2;
            else
                scaledValue = height / 2;

            return scaledValue;
        }

        private void Animation_Click(object sender, RoutedEventArgs e)
        {
            mapOfESP.Children.Clear();
        }
    }
}
