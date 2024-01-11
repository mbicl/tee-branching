#include <gst/gst.h>
#include <string.h>

typedef struct {
    GstElement
        *pipeline,
        *src,
        *tee,

        *queue1,
        *sink1,

        *queue2,
        *sink2;
    
    GMainLoop *loop;
} CustomData;


static void error_callback(GstBus *bus, GstMessage *msg, CustomData *data){
    GError *err;
    gchar *debug;

    gst_message_parse_error(msg,&err,&debug);

    g_printerr("ERROR received from element %s: %s\n",GST_OBJECT_NAME(msg->src),err->message);
    g_printerr("Debugging information: %s\n",debug);
    g_clear_error(&err);
    g_free(debug);

    g_main_loop_quit(data->loop);
}

int main(int argc, char *argv[]){
    CustomData data;
    GstPad
        *tee_pad1,
        *tee_pad2,

        *queue_pad1,
        *queue_pad2;
    
    GstBus *bus;

    memset(&data,0,sizeof(data));

    gst_init(&argc,&argv);

    /* Create elements */
    data.pipeline = gst_pipeline_new("test-pipeline");
    data.tee = gst_element_factory_make("tee","tee");
    data.src = gst_element_factory_make("videotestsrc","videosrc");
    
    data.queue1 = gst_element_factory_make("queue","queue1");
    data.sink1 = gst_element_factory_make("autovideosink","sink1");

    data.queue2 = gst_element_factory_make("queue","queue2");
    data.sink2 = gst_element_factory_make("filesink","sink2");

    if (
        !data.pipeline ||
        !data.src ||
        !data.tee ||
        !data.queue1 ||
        !data.sink1 ||
        !data.queue2 ||
        !data.sink2
    ) {
        g_printerr("All elements could not be created. Exiting...\n");
        return -1;
    }

    g_object_set(data.src,"pattern",1,NULL);
    g_object_set(data.sink2,"location","output.mp4",NULL);
    
    gst_bin_add_many(
        GST_BIN(data.pipeline),
        data.src,
        data.tee,
        data.queue1,
        data.queue2,
        data.sink1,
        data.sink2,
        NULL
    );

    if (
        !gst_element_link_many(data.src,data.tee,NULL) ||
        !gst_element_link_many(data.queue1,data.sink1,NULL) ||
        !gst_element_link_many(data.queue2,data.sink2,NULL)
    ) {
        g_printerr("Elements could not be linked. Exiting...\n");
        gst_object_unref(data.pipeline);
        return -1;
    }

    /* Manually linking the Tee, which has "Request" pads */
    tee_pad1 = gst_element_get_request_pad(data.tee,"src_%u");
    g_print("Obtained request pad %s for the 1st branch.\n",gst_pad_get_name(tee_pad1));
    queue_pad1 = gst_element_get_static_pad(data.queue1,"sink");

    tee_pad2 = gst_element_get_request_pad(data.tee,"src_%u");
    g_print("Obtained request pad %s for the 2nd branch.\n",gst_pad_get_name(tee_pad2));
    queue_pad2 = gst_element_get_static_pad(data.queue2,"sink");

    if (
        gst_pad_link(tee_pad1,queue_pad1) != GST_PAD_LINK_OK ||
        gst_pad_link(tee_pad2,queue_pad2) != GST_PAD_LINK_OK 
    ) {
        g_print("Tee could not be linked to branches. Exiting...\n");
        gst_object_unref(data.pipeline);
        return -1;
    }
    gst_object_unref(queue_pad2);
    gst_object_unref(queue_pad1);

    bus = gst_element_get_bus(data.pipeline);
    gst_bus_add_signal_watch(bus);
    g_signal_connect(G_OBJECT(bus),"message::error",(GCallback)error_callback,&data);
    gst_object_unref(bus);

    gst_element_set_state(data.pipeline,GST_STATE_PLAYING);

    data.loop = g_main_loop_new(NULL,FALSE);
    g_main_loop_run(data.loop);

    /* Release the request pads from the Tee, and unref them */
    gst_element_release_request_pad(data.tee,tee_pad1);
    gst_element_release_request_pad(data.tee,tee_pad2);
    gst_object_unref(tee_pad1);
    gst_object_unref(tee_pad2);

    /* Free resources */
    g_print("Stopping pipeline...\n");
    gst_element_set_state(data.pipeline,GST_STATE_NULL);
    gst_object_unref(data.pipeline);
    return 0;
}