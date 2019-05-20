using System;
using System.Collections;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using UnityEngine;
using HoloToolkit.Unity.SpatialMapping;
using HoloToolkit.Unity.InputModule;
using HoloToolkit.Unity;
using UnityEngine.XR.WSA.Input;

#if NETFX_CORE
using Stopwatch = System.Diagnostics.Stopwatch;
using System.Collections.Generic;
using Windows.Foundation;
using Windows.Graphics.Imaging;
using Windows.Media.Capture;
using Windows.Media.Capture.Frames;
using Windows.Media.MediaProperties;
using Windows.Storage;
using Windows.Storage.Streams;
#endif

public class ScriptProva : MonoBehaviour
{

    public GameObject bbox;
    public GameObject label;
    private TextMesh labelTM;
    private int photoIndex = 0;
    private bool ToJPEG = true;
    //private Renderer render;
    //private TextMesh textmesh;

    private class IRO
    {
        public double x;
        public double y;
        public double w;
        public double h;
        public string name;
        public Vector3 z;
        public int photoIndex;

        public IRO(Vector3 currentHit)
        {
            z = currentHit;
        }

        public IRO(int photoIndex, Vector3 currentHit)
        {
            this.photoIndex = photoIndex;
            this.z = currentHit;
        }
    }



#if NETFX_CORE
    private const int OVERHEAD_SIZE = 12; // STX:4,TYPE:1,INDEX:4,LENGTH:4,BUFFER:x,EXT:4
    private const int STX = 767590; //2,7,0,6,1,9,9,2=10,111,0,110,1,1001,1001,10=101110110110011001, int signed little order
    private const int ETX = 170807; //24121953
    private static byte[] STXb = BitConverter.GetBytes(STX);
    private static byte[] ETXb = BitConverter.GetBytes(ETX);
    private int startProcessing = 0;
    private IPEndPoint outEP;
    public SocketAsyncEventArgs recv_sae;
    private string[] categories;
    private uint buffer_size;
    private byte[] buffer;
    private int timeouts;
    private Socket socket;
    private int socketIsBusy;
    private Dictionary<int, IRO> iros = new Dictionary<int, IRO>(3);
    private Config config = new Config();
    private MediaFrameReader frameReader;
    private InMemoryRandomAccessStream inMemStream;
    private BitmapEncoder encoder;
    private Vector3 hitPosition;
    private Camera cameraMain;
    private IRO iro;
    private SoftwareBitmap sbmp;
    private byte[] rbuffer;
    private SocketError send_sae_error;
    private SocketError recv_sae_error;
    private GestureRecognizer recognizer;
    private int isEncoding;
    private int tapped;
    private GameObject photoBBox;
    private GameObject quad;
    private Renderer quadRenderer;
    private GameObject quadTM;
    private float distance;
    private int capturePhoto;
    private int processing;

    //private GameObject ego;

    async void Start()
    {
        //ego = Instantiate(bbox, new Vector3(0, 0, 10), new Quaternion(0, 0, 0, 0)) as GameObject;
        //var tm = ego.transform.GetChild(1).gameObject.GetComponent<TextMesh>();
        //var pl = ego.transform.GetChild(0).gameObject;
        //ego.transform.localScale = new Vector3(0.2f, 0.2f, 0.2f);
        //pl.transform.localScale = new Vector3(0.3f, 0.3f, 0.3f);
        //tm.text = "ciao";

        tapped = 0;
        startProcessing = 0;

        rbuffer = new byte[256];
        enabled = false;
        //render = gameObject.GetComponent<Renderer>();
        //textmesh = gameObject.GetComponentInChildren<TextMesh>();
        //render.material.color = Color.blue;
        await StartCapturer();
        await StartSocket();
        startProcessing = 0;
        await frameReader.StartAsync();
        Debug.Log("Socket and FrameReader started!!!");


        //Debug.Log("Starting MyCouroutine...");
        //mycoroutine = StartCoroutine("MyCoroutine");


        // Subscribing to the Microsoft HoloLens API gesture recognizer to track user gestures
        recognizer = new GestureRecognizer();
        recognizer.SetRecognizableGestures(GestureSettings.Tap);
        recognizer.Tapped += TapHandler;
        recognizer.StartCapturingGestures();

        enabled = true;
    }

    async void Update()
    {
        if (socket.Connected == false) {
            socket.Dispose();
            await StartSocket();
        }
        //if (startProcessing == 1) {
        //    startProcessing = 0;
        if (startProcessing == 1 && 0 == Interlocked.CompareExchange(ref processing, 1, 0)) {
            //startProcessing = 0;
            processing = 1;
            StartCoroutine("MyCoroutine");
        }
    }

    private void TapHandler(TappedEventArgs obj)
    {
        if (1 == Interlocked.CompareExchange(ref capturePhoto, 1, 0))
        {
            return;
        }

        hitPosition = GazeManager.Instance.HitPosition;
        cameraMain = CameraCache.Main;//GazeManager.Instance.HitNormal;

        photoBBox = Instantiate(bbox, hitPosition, Quaternion.identity) as GameObject;

        distance = Vector3.Distance(hitPosition, cameraMain.transform.position);

        photoBBox.transform.localScale = 0.6f * photoBBox.transform.localScale.Mul(new Vector3(distance, distance, distance));
        photoBBox.transform.position = hitPosition;
        photoBBox.transform.forward = cameraMain.transform.forward;
        photoBBox.transform.Rotate(-photoBBox.transform.eulerAngles.x, 0, -photoBBox.transform.eulerAngles.z);

        quad = photoBBox.transform.Find("Quad").gameObject;
        quadRenderer = quad.GetComponent<Renderer>() as Renderer;
        label = photoBBox.transform.Find("Label").gameObject;
        labelTM = label.GetComponent<TextMesh>();
        labelTM.text = "trying...";
    }

    private void OnFrameArrived(MediaFrameReader sender, MediaFrameArrivedEventArgs args)
    {
        if (0 == capturePhoto) {
            return;
        }

        try {
            using (var frame = sender.TryAcquireLatestFrame()) {
                if (frame != null && frame.VideoMediaFrame != null) {
                    sbmp = frame.VideoMediaFrame.SoftwareBitmap;// SoftwareBitmap.Convert(frame.VideoMediaFrame.SoftwareBitmap, BitmapPixelFormat.Bgra8, BitmapAlphaMode.Ignore);
                    Interlocked.Exchange(ref startProcessing, 1);
                }
            }
        }
        catch (Exception e) {
            Debug.Log("OnFramerArrived: " + e);
        }
    }

    IEnumerator MyCoroutine()
    {
        bool isAsync;
        int size;

        Code();

        Interlocked.Exchange(ref isEncoding, 1);
        var task = encoder.FlushAsync();
        task.Completed += new AsyncActionCompletedHandler((IAsyncAction source, AsyncStatus status) => {
            Interlocked.Exchange(ref isEncoding, 0);
        });
        do {
            yield return null;
        } while (1 == Interlocked.CompareExchange(ref isEncoding, 0, 0));
        sbmp = null;

        yield return null;

        size = PreparePacket();

        yield return null;

        #region sending
        int sent;
        using (var send_sae = new SocketAsyncEventArgs()) {

            isAsync = false;
            try {
                send_sae.Completed += transfer_callback;
                send_sae.SetBuffer(buffer, 0, size + OVERHEAD_SIZE);
                Interlocked.Exchange(ref socketIsBusy, 1);
                isAsync = socket.SendAsync(send_sae);
            }
            catch (SocketException e) {
                Debug.LogFormat("Receiving timeout." + e.SocketErrorCode.ToString());
                yield break;
            }
            do {
                yield return null;
            } while (1 == Interlocked.CompareExchange(ref socketIsBusy, 0, 0));
            if (!isAsync) yield break;

            if (send_sae_error != SocketError.Success) {
                Debug.Log("Socket::Send: error = " + send_sae_error);
                yield break;
            }
            sent = send_sae.BytesTransferred;
        }
        if (sent < size + OVERHEAD_SIZE) {
            Debug.LogFormat("Error not sent {0} bytes", size + OVERHEAD_SIZE - sent);
            yield break;
        }
        inMemStream.Dispose();
        #endregion

        #region receiving
        isAsync = false;
        try {
            Interlocked.Exchange(ref socketIsBusy, 1);
            isAsync = socket.ReceiveAsync(recv_sae);
        }
        catch (Exception e) {
            Debug.Log(e.Message);
        }
        do {
            yield return null;
        } while (1 == Interlocked.CompareExchange(ref socketIsBusy, 0, 0));
        if (!isAsync) yield break;
        if (recv_sae_error != SocketError.Success) {
            Debug.Log("Socket::Send: error = " + recv_sae_error);
            yield break;
        }
        #endregion

        HandleBBox(recv_sae.BytesTransferred);

        ++photoIndex;

        Interlocked.Exchange(ref capturePhoto, 0);
        Interlocked.Exchange(ref startProcessing, 0);
        Interlocked.Exchange(ref processing, 0);

        yield return null;
    }

    private void HandleBBox(int rl)
    {
        int index, nbbox, stx;

        if (rl == 0) {
            Debug.Log($"Too few data received ({rl}).");
            return;
        }
        try {
            BinaryReader breader;
            try {
                breader = new BinaryReader(new MemoryStream(recv_sae.Buffer));
            }
            catch (Exception e) {
                Debug.Log("HandleBBox: " + e);
                return;
            }
            stx = breader.ReadInt32();
            index = breader.ReadInt32();
            nbbox = breader.ReadInt32();

            if (stx != STX) {
                Debug.Log(string.Format("wrong STX ({0,5} != {1,5}.", stx, STX));
            }
            else
            if (index != photoIndex) {
                Debug.LogFormat(string.Format("Wrong Index ({0,5} != {1,5}.", index, iro.photoIndex));
            }
            else
            if (nbbox > 0) {
                for (int nb = 0; nb < nbbox; ++nb) {
                    float x = breader.ReadSingle(); //is the x coordinate of the bbox center
                    float y = breader.ReadSingle(); //is the y coordinate of the bbox center
                    float w = breader.ReadSingle(); //is the width of the bbox
                    float h = breader.ReadSingle(); //is the height of the bbox
                    float o = breader.ReadSingle();
                    float p = breader.ReadSingle();
                    int cindex = breader.ReadInt32();

                    Debug.LogFormat("[{0,5}:{1}]:\tx={2,6:F4}, y={3,6:F4}, w={4,6:F4}, y={5,6:F4}, o={6,6:F4}, p={7,6:F4}, name#{8,2}=" + categories[cindex] + ".", photoIndex, nb, x, y, w, h, o, p, cindex);

                    GenBBox(w, h, x, y, cindex);
                }
            }
            else
            if (nbbox == 0) {
                Destroy(photoBBox);
                Debug.Log("[{0:5}] No bbox found.");
            }
            else {
                Debug.LogFormat("[{0:5}]: error during inference.", photoIndex);
                //iros.Remove(index);
            }
        }
        catch {
            Debug.Log("HandleBBox error.");
        }
        finally {
            Interlocked.Exchange(ref tapped, 0);
            Destroy(photoBBox);
        }
    }

    private void GenBBox(float w, float h, float x, float y, int cindex)
    {
        var z = distance;

        w *= z;
        h *= z;
        x *= z;
        y *= z;

        var box = Instantiate(bbox, hitPosition, Quaternion.identity) as GameObject;

        var quat = cameraMain.transform.rotation;
        box.transform.forward = quat * Vector3.forward;
        box.transform.Rotate(-box.transform.eulerAngles.x, 0, -box.transform.eulerAngles.z); // makes bbox vertical

        var quad = box.transform.Find("Quad").gameObject;
        var quadRenderer = quad.GetComponent<Renderer>() as Renderer;
        Bounds quadBounds = quadRenderer.bounds;

        quad.transform.localScale = new Vector3(w, h, 1);

        quadBounds.size.Set(w, y, quadBounds.size.z);

        //box.transform.position -= new Vector3(x, y, 0);

        Debug.Log($"{z}\n{x}, {y}, {w}, {h}\n{quadBounds.size}\n{quadBounds.size.normalized}\n{quadBounds}\n{quad.transform.localScale}");

        box.transform.Find("Label").gameObject.GetComponent<TextMesh>().text = categories[cindex];
    }

    private int PreparePacket()
    {
        Stream stream;
        int offset = CEEnum.BMP == config.Encoder ? 54 : 0;
        int size = (int)inMemStream.Size - offset;

        stream = inMemStream.AsStreamForRead();
        stream.Seek(offset, SeekOrigin.Begin);

        int read_size = stream.Read(buffer, OVERHEAD_SIZE, size); //removing BMP header
        if (size != read_size) { throw new Exception(string.Format("Reading error: read {0} on {1}.", read_size, size)); }

        Array.Copy(BitConverter.GetBytes(photoIndex), 0, buffer, 4, 4);
        Array.Copy(BitConverter.GetBytes(size), 0, buffer, 8, 4);

        //Debug.Log(string.Format("\nSending {0} bytes, index {1}, STX {2}.\n", size, BitConverter.ToInt32(buffer, 4), BitConverter.ToInt32(buffer, 0)));
        return size;
    }

    private async Task StartSocket()
    {
        IPAddress ipa = IPAddress.Parse(config.ip);
        outEP = new IPEndPoint(ipa, config.OutPort);
        recv_sae = new SocketAsyncEventArgs();
        recv_sae.Completed += transfer_callback;
        socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
        Interlocked.Exchange(ref socketIsBusy, 0);

        while (true) {
            try {
                bool isAsync = false;
                //textmesh.text = "trying...";
                Debug.Log(string.Format("Trying to connect to {0}...", outEP));
                var connect_sae = new SocketAsyncEventArgs();

                Interlocked.Exchange(ref socketIsBusy, 1);
                connect_sae.Completed += connect_callback; ///socket_(s, e) => autoResetEvent.Set();
                connect_sae.RemoteEndPoint = outEP;
                isAsync = socket.ConnectAsync(connect_sae);

                while (isAsync && 1 == Interlocked.CompareExchange(ref socketIsBusy, 0, 0)) {
                    await Task.Delay(500);
                }

                if (socket.Connected) break;
            }
            catch (SocketException e) {
                //textmesh.text = "trying...failed";
                Debug.Log(string.Format("failed: {0}\n", e));
            }
        }

        //Debug.Log("connected.");

        await SendConfig();
    }

    private void connect_callback(object sender, SocketAsyncEventArgs e)
    {
        //Debug.Log("Args_Completed:\t" + e.LastOperation + " => " + e.SocketError);
        Interlocked.Exchange(ref socketIsBusy, 0);
    }

    private void transfer_callback(object sender, SocketAsyncEventArgs e)
    {
        //Debug.LogFormat("Args_Completed:\t{0} => {1}   [{2}].", e.LastOperation.ToString(), e.SocketError.ToString(), e.BytesTransferred);
        if (e.LastOperation == SocketAsyncOperation.Send)
            send_sae_error = e.SocketError;
        if (e.LastOperation == SocketAsyncOperation.Receive)
            recv_sae_error = e.SocketError;
        Interlocked.Exchange(ref socketIsBusy, 0);
    }

    private async Task SendConfig()
    {
        byte[] config_buffer = config.GetBuffer(STX);

        //send_sae.SetBuffer(config_buffer, 0, config_buffer.Length);
        //Debug.Log("Sending config buffer...");
        await SendAsync(config_buffer, config_buffer.Length);
        //Debug.Log("sent!");

        config_buffer[0] = 0;

        recv_sae.SetBuffer(config_buffer, 0, 12);
        int r = await Receive();

        int ln = 0, nn;
        if (BitConverter.ToInt32(config_buffer, 0) == STX) {
            ln = BitConverter.ToInt32(config_buffer, 4);
            nn = BitConverter.ToInt32(config_buffer, 8);
        }
        else {
            throw new SocketException();
        }

        byte[] cbuf = new byte[ln];
        recv_sae.SetBuffer(cbuf, 0, ln);
        ln -= await Receive();
        if (ln > 0) throw new SocketException();

        BinaryReader br = new BinaryReader(new MemoryStream(cbuf));

        categories = new string[nn];

        StringBuilder sb = new StringBuilder("");
        for (int i = 0; i < nn; i++) {
            while (true) {
                char c = br.ReadChar();
                if (c == '\0') {
                    categories[i] = sb.ToString();
                    sb.Clear();
                    break;
                }
                sb.Append(c);
            }
        }

        buffer_size = (OVERHEAD_SIZE + (config.FinalResolution.Width * config.FinalResolution.Height * 3)) >> (config.Encoder == CEEnum.BMP ? 0 : 4);
        buffer = new byte[buffer_size + 10];

        recv_sae.SetBuffer(0, 150);

        Array.Copy(STXb, buffer, 4);

        Debug.Log($"Buffer length: {buffer.Length}\n");
    }

    public async Task<int> SendAsync(byte[] b, int l)
    {
        try {
            bool isAsync = false;
            var send_sae = new SocketAsyncEventArgs();
            send_sae.Completed += transfer_callback;
            send_sae.SetBuffer(b, 0, l);
            Interlocked.Exchange(ref socketIsBusy, 1);
            isAsync = socket.SendAsync(send_sae);
            while (isAsync && 1 == Interlocked.CompareExchange(ref socketIsBusy, 0, 0)) {
                await Task.Delay(5);
            }
            if (send_sae.SocketError != SocketError.Success) Debug.Log("Socket::Send: error = " + send_sae.SocketError);
            int sl = send_sae.BytesTransferred;
            send_sae.Dispose();
            return sl;
        }
        catch (Exception e) {
            Debug.Log("Socket::Send:\t" + e.Message);
        }
        return -1;
    }

    public async Task<int> Receive()
    {
        try {
            bool isAsync = false;
            Interlocked.Exchange(ref socketIsBusy, 1);
            isAsync = socket.ReceiveAsync(recv_sae);
            while (isAsync && 1 == Interlocked.CompareExchange(ref socketIsBusy, 0, 0)) {
                await Task.Delay(5);
            }
            return recv_sae.BytesTransferred;
        }
        catch (Exception e) {
            Debug.Log(e.Message);
        }
        return -1;
    }

    private async Task StartCapturer()
    {
        MediaFrameSource mediaFrameSource;
        var allGroups = await MediaFrameSourceGroup.FindAllAsync();
        if (allGroups.Count <= 0) {
            //textmesh.text = "Orca";
            Debug.Log("Orca");
        }
        var mediaCapture = new MediaCapture();
        var settings = new MediaCaptureInitializationSettings
        {
            SourceGroup = allGroups[0],
            SharingMode = MediaCaptureSharingMode.SharedReadOnly,
            StreamingCaptureMode = StreamingCaptureMode.Video,
            MemoryPreference = MediaCaptureMemoryPreference.Cpu
        };

        await mediaCapture.InitializeAsync(settings);

        //render.material.color = new Color(0, 0, 0.5f);

        mediaFrameSource = mediaCapture.FrameSources.Values.Single(x => x.Info.MediaStreamType == MediaStreamType.VideoRecord);
        try {
            MediaFrameFormat targetResFormat = null;
            foreach (var f in mediaFrameSource.SupportedFormats.OrderBy(x => x.VideoFormat.Width * x.VideoFormat.Height)) {
                //textmesh.text = string.Format("{0}x{1} {2}/{3}", f.VideoFormat.Width, f.VideoFormat.Height, f.FrameRate.Numerator, f.FrameRate.Denominator);
                if (f.VideoFormat.Width == 896 && f.VideoFormat.Height == 504 && f.FrameRate.Numerator == 24) {
                    targetResFormat = f;
                }
            }
            await mediaFrameSource.SetFormatAsync(targetResFormat);
        }
        catch {
            //textmesh.text = "Orca2";
        }

        try {
            frameReader = await mediaCapture.CreateFrameReaderAsync(mediaFrameSource, MediaEncodingSubtypes.Bgra8);
            frameReader.AcquisitionMode = MediaFrameReaderAcquisitionMode.Realtime;
            frameReader.FrameArrived += OnFrameArrived;
        }
        catch {
            //textmesh.text = "Orca3";
        }
    }


    private void Code()
    {
        inMemStream = new InMemoryRandomAccessStream();
        if (config.JpegQuality > 0) {
            BitmapPropertySet propertySet = new BitmapPropertySet();
            propertySet.Add("ImageQuality", new BitmapTypedValue(config.JpegQuality, Windows.Foundation.PropertyType.Single));
#if CODING_AWAIT
            encoder = await BitmapEncoder.CreateAsync(config.EncoderGuid, inMemStream, propertySet);
#else
            encoder = BitmapEncoder.CreateAsync(config.EncoderGuid, inMemStream, propertySet).AsTask().GetAwaiter().GetResult();
#endif
        }
        else {
#if CODING_AWAIT
            encoder = await BitmapEncoder.CreateAsync(config.EncoderGuid, inMemStream);
#else
            encoder = BitmapEncoder.CreateAsync(config.EncoderGuid, inMemStream).AsTask().GetAwaiter().GetResult();
#endif
        }
        encoder.BitmapTransform.Flip = BitmapFlip.Vertical;
        switch (config.Transform) {
            case CTEnum.CROPPED:
                encoder.BitmapTransform.Bounds = new BitmapBounds()
                {
                    X = (config.OriginalResolution.Width - config.FinalResolution.Width) >> 1,
                    Y = (config.OriginalResolution.Height - config.FinalResolution.Height) >> 1,
                    Width = config.FinalResolution.Width,
                    Height = config.FinalResolution.Height
                };
                break;
            case CTEnum.SCALED:
                encoder.BitmapTransform.InterpolationMode = BitmapInterpolationMode.Linear;
                encoder.BitmapTransform.ScaledWidth = config.FinalResolution.Width;
                encoder.BitmapTransform.ScaledHeight = config.FinalResolution.Height;
                break;
            case CTEnum.SQUARED:
                uint h = config.OriginalResolution.Height;
                encoder.BitmapTransform.Bounds = new BitmapBounds()
                {
                    X = (config.OriginalResolution.Width - h) >> 1,
                    Y = h,
                    Width = h,
                    Height = h
                };
                encoder.BitmapTransform.InterpolationMode = BitmapInterpolationMode.Linear;
                encoder.BitmapTransform.ScaledWidth = config.FinalResolution.Width;
                encoder.BitmapTransform.ScaledHeight = config.FinalResolution.Height;
                break;
        }
            sbmp = SoftwareBitmap.Convert(sbmp, BitmapPixelFormat.Bgra8, BitmapAlphaMode.Ignore);
        encoder.SetSoftwareBitmap(sbmp);
#if CODING_AWAIT
        //await encoder.FlushAsync();
#else
        //encoder.FlushAsync().AsTask().GetAwaiter().GetResult();
#endif
    }

    async Task Send()
    {

        #region socket
        Stream stream;
        int offset = CEEnum.BMP == config.Encoder ? 54 : 0;
        int size = (int)inMemStream.Size - offset;

        stream = inMemStream.AsStreamForRead();
        stream.Seek(offset, SeekOrigin.Begin);

        int read_size = stream.Read(buffer, OVERHEAD_SIZE, size); //removing BMP header
        if (size != read_size) { throw new Exception(string.Format("Reading error: read {0} on {1}.", read_size, size)); }

        Array.Copy(BitConverter.GetBytes(photoIndex), 0, buffer, 4, 4);
        Array.Copy(BitConverter.GetBytes(size), 0, buffer, 8, 4);

        Debug.Log(string.Format("\nSending {0} bytes, index {1}, STX {2}.\n", size, BitConverter.ToInt32(buffer, 4), BitConverter.ToInt32(buffer, 0)));

        try {
            int sent = await SendAsync(buffer, size + OVERHEAD_SIZE);

            if (sent < size + OVERHEAD_SIZE) {
                Debug.LogFormat("Error not sent {0} bytes", size + OVERHEAD_SIZE - sent);
            }

            inMemStream.Dispose();
            ++photoIndex;
        }
        catch (SocketException e) {
            Debug.LogFormat("Receiving timeout, " + e.SocketErrorCode.ToString());
            if (++timeouts == 20) {
                socket.Dispose();
                await StartSocket();
                timeouts = 0;
                return; // no interlock
            }
        }
        #endregion
        catch (Exception e) {
            Debug.LogFormat("Error in buffer: [{0}].", e.Message);
        }
        finally {

            Debug.Log(string.Format("###########end {0}", photoIndex));

            //Interlocked.Exchange(ref isProcessFrameBusy, 0);
        }
    }


    /// <summary>
    /// This method hosts a series of calculations to determine the position 
    /// of the Bounding Box on the quad created in the real world
    /// by using the Bounding Box received back alongside the Best Prediction
    /// </summary>
    public Vector3 CalculateBoundingBoxPosition(Bounds b, float x, float y, float w, float h)
    {

        double centerFromLeft = 1;// x + (w / 2);
        double centerFromTop = 1; // y + (h / 2);

        double quadWidth = b.size.normalized.x;
        double quadHeight = b.size.normalized.y;

        double normalisedPos_X = (quadWidth * centerFromLeft) - (quadWidth / 2);
        double normalisedPos_Y = (quadHeight * centerFromTop) - (quadHeight / 2);

        Debug.Log($"BB: left {x}, top {y}, width {w}, height {h}\nBB CenterFromLeft {centerFromLeft}, CenterFromTop {centerFromTop}\nQuad Width {b.size.normalized.x}, Quad Height {b.size.normalized.y}");
        return new Vector3((float)normalisedPos_X, (float)normalisedPos_Y, 0);
    }
}

class Config
{
    public bool IsSync = true;

    public int TestsNumber = 150;

    public bool IsUDP = false;
    public int BufferSize = 65536;
    public int OutPort = 56789;
    public int InPort = 56788;
    public string ip = "10.3.141.1";
    public bool ReceivingEnabled = false;

    public bool SaveToFile = true;

    public CEEnum Encoder = CEEnum.BMP;
    public Guid EncoderGuid { get { return Encoder == CEEnum.JPEG ? BitmapEncoder.JpegEncoderId : BitmapEncoder.BmpEncoderId; } }

    public double JpegQuality = -1;
    public CTEnum Transform = CTEnum.CROPPED;
    public Resolution FinalResolution = new Resolution(416, 416, 24);

    public Resolution OriginalResolution = new Resolution(896, 504, 24);


    public string EncoderName { get { return Encoder == CEEnum.BMP ? "BMP" : "JPEG"; } }

    public string[] TransformName = new string[] { "none", "cropped", "scaled" };

    public Config()
    {
        if (FinalResolution.Width == 0 && FinalResolution.Height == 0) FinalResolution = OriginalResolution;
        else if (FinalResolution.Height == 0) {
            FinalResolution.Height =
                Convert.ToUInt32(Convert.ToDouble(OriginalResolution.Height) * Convert.ToDouble(FinalResolution.Width)
                / Convert.ToDouble(OriginalResolution.Width));
        }
    }

    public byte[] GetBuffer(int stx)
    {
        int l = 0;
        byte[] buffer = new byte[16];

        Array.Copy(BitConverter.GetBytes(stx), 0, buffer, l, 4); l += 4;
        Array.Copy(BitConverter.GetBytes(FinalResolution.Height), 0, buffer, l, 4); l += 4; // rows
        Array.Copy(BitConverter.GetBytes(FinalResolution.Width), 0, buffer, l, 4); l += 4;  // cols
        Array.Copy(BitConverter.GetBytes((Encoder == CEEnum.BMP ? 1 : 0)), 0, buffer, l, 4); l += 4;

        return buffer;
    }
}

public enum CEEnum
{
    JPEG, BMP
}

public enum CTEnum
{
    NONE, CROPPED, SCALED, SQUARED
}

#pragma warning disable CS0660 // Type defines operator == or operator != but does not override Object.Equals(object o)
#pragma warning disable CS0661 // Type defines operator == or operator != but does not override Object.GetHashCode()
class Resolution
#pragma warning restore CS0661 // Type defines operator == or operator != but does not override Object.GetHashCode()
#pragma warning restore CS0660 // Type defines operator == or operator != but does not override Object.Equals(object o)
{
    public uint Width;
    public uint Height;
    public uint FrameRate;
    public uint size;
    public Resolution(uint w, uint h, uint f) { Width = w; Height = h; FrameRate = f; size = w * h * 3; }

    public override string ToString()
    {
        return Width + "x" + Height;
    }

    public static bool operator ==(Resolution a, Resolution b)
    {
        return a.Width == b.Width && a.Height == b.Height;
    }

    public static bool operator !=(Resolution a, Resolution b)
    {
        return a.Width != b.Width || a.Height != b.Height;
    }

    public bool Compare(MediaFrameFormat mff)
    {
        return mff.VideoFormat.Width == Width && mff.VideoFormat.Height == Height && mff.VideoFormat.MediaFrameFormat.FrameRate.Numerator == FrameRate;
    }

    public void SetHeight(uint h) { this.Height = h; }
#endif
}
