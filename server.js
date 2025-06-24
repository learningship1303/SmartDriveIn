const express = require('express');
const mongoose = require('mongoose');
const bodyParser = require('body-parser');
const cors = require('cors');

// Setup
const app = express();
const PORT = process.env.PORT || 3000;
app.use(cors());
app.use(bodyParser.json());

// MongoDB Connection
mongoose.connect('mongodb://localhost:27017/npr_data', {
  useNewUrlParser: true,
  useUnifiedTopology: true,
});

const db = mongoose.connection;
db.on('error', console.error.bind(console, 'MongoDB connection error:'));
db.once('open', () => {
  console.log('Connected to MongoDB');
});

// Mongoose Schema
const PlateSchema = new mongoose.Schema({
  numberPlate: String,
  imageUrl: String,
  timestamp: { type: Date, default: Date.now },
});

const Plate = mongoose.model('Plate', PlateSchema);

// API Route
app.post('/api/save-plate', async (req, res) => {
  const { numberPlate, imageUrl } = req.body;

  if (!numberPlate) return res.status(400).json({ error: 'Number Plate is required' });

  try {
    const newEntry = new Plate({ numberPlate, imageUrl });
    await newEntry.save();
    res.json({ success: true, message: 'Plate data saved successfully' });
  } catch (error) {
    res.status(500).json({ success: false, message: 'Error saving data', error });
  }
});

// Start server
app.listen(PORT, () => {
  console.log(`Server running on http://localhost:${PORT}`);
});
//optional
app.get('/api/plates', async (req, res) => {
    const plates = await Plate.find().sort({ timestamp: -1 });
    res.json(plates);
  });
  